#include "voimpl.h"
#include "load_calib.h"
#include <ppl.h>

VOImpl::VOImpl()
{
	m_fx = 0.0f;
	m_fy = 0.0f;
	m_cx = 0.0f;
	m_cy = 0.0f;
}

VOImpl::~VOImpl()
{

}

void VOImpl::Setup(const SlamParameters& parameters)
{
	const ICPParamters& icp_param = parameters.icp_param;
	trimesh::CameraData camera_data;
	if (!load_camera_data_from_file(icp_param.calib_file, camera_data))
	{
		std::cout << "Cabli Data Error." << std::endl;
	}

	m_fx = camera_data.m_fx;
	m_fy = camera_data.m_fy;
	m_cx = camera_data.m_cx;
	m_cy = camera_data.m_cy;

	m_icp.reset(new trimesh::ProjectionICP(m_fx, m_fy, m_cx, m_cy));

	const OctreeParameters& oct_param = parameters.octree_param;
	int cell_depth = oct_param.cell_depth;
	if (cell_depth < 2) cell_depth = 2;
	else if (cell_depth > 6) cell_depth = 6;

	float cell_resolution = 0.0f;
	if (cell_depth == 2) cell_resolution = 1.6f;
	if (cell_depth == 3) cell_resolution = 0.8f;
	if (cell_depth == 4) cell_resolution = 0.4f;
	if (cell_depth == 5) cell_resolution = 0.2f;
	if (cell_depth == 6) cell_resolution = 0.1f;
	m_octree.reset(new Octree(cell_depth, cell_resolution));

	m_layers.resize(8000000, 0);
}

void VOImpl::SetVOTracer(VOTracer* tracer)
{
	m_tracer = tracer;
}

void VOImpl::ProcessOneFrame(TriMeshPtr& mesh, LocateData& locate_data)
{
	m_state.IncFrame();
	mesh->frame = m_state.Frame();

	LocateOneFrame(mesh, locate_data);

	if (!locate_data.lost)
		FusionFrame(mesh, locate_data);
}

void VOImpl::LocateOneFrame(TriMeshPtr& mesh, LocateData& locate_data)
{
	if (m_state.FirstFrame())
	{//first frame
		locate_data.lost = false;
		m_state.SetFirstFrame(false);
	}
	else
	{
		bool result = Frame2Frame(mesh);
		if (result)
		{
			locate_data.locate_type = 0;
			std::cout << mesh->frame << " continous success." << std::endl;
		}
		else
		{
			locate_data.locate_type = 1;
			result = Relocate(mesh);
		}

		if(!result) std::cout << mesh->frame << " lost." << std::endl;
		locate_data.lost = !result;
	}
}

void VOImpl::FusionFrame(TriMeshPtr& mesh, const LocateData& locate_data)
{
	if (!m_octree->m_initialized)
		m_octree->Initialize(mesh->bbox.center());

	std::vector<int> indexes(mesh->vertices.size(), -1);
	m_octree->Insert(mesh->vertices, mesh->normals, mesh->global, indexes);

	int new_count = 0;
	size_t vsize = mesh->vertices.size();
	for (size_t i = 0; i < vsize; ++i)
	{
		int index = indexes.at(i);
		if (index >= 0 && m_layers.at(index) == 0)
		{
			++new_count;
		}
	}

	bool use_as_keyframe = false;
	if ((float)new_count / (float)vsize > 0.5f)
		use_as_keyframe = true;
	if (use_as_keyframe)
	{
		for (size_t i = 0; i < vsize; ++i)
		{
			int index = indexes.at(i);
			if (index >= 0)
			{
				++m_layers.at(index);
			}
		}
	}
	SetLastMesh(mesh, use_as_keyframe);

	std::cout << "Octree nodes " << m_octree->m_current_index <<
		" points " << m_octree->m_current_point_index << std::endl;
	if (m_tracer)
	{
		//RenderData* render_data = new RenderData();
		//render_data->mesh = mesh;
		//m_tracer->OnFrame(render_data);

		PatchRenderData* patch_data = new PatchRenderData();
		patch_data->indices.swap(indexes);
		patch_data->xf = mesh->global;
		patch_data->step = locate_data.locate_type;
		size_t size = patch_data->indices.size();
		patch_data->normals.resize(size);
		patch_data->points.resize(size);
		for (size_t i = 0; i < size; ++i)
		{
			int index = patch_data->indices.at(i);
			if (index >= 0)
			{
				patch_data->points.at(i) = m_octree->m_points.at(index);
				patch_data->normals.at(i) = m_octree->m_normals.at(index);
			}
		}

		m_tracer->OnFrame(patch_data);
	}
}

bool VOImpl::Frame2Frame(TriMeshPtr& mesh)
{
	//frame 2 frame
	trimesh::xform xf;
	m_icp->SetSource(mesh.get());
	m_icp->SetTarget(m_last_mesh.get());
	float err_p = m_icp->Do(xf);
	if (err_p > 0.1f || err_p < 0.0f)
		return false;

	mesh->global = xf * m_last_mesh->global;
	//frame 2 model
	return true;
}

bool VOImpl::Relocate(TriMeshPtr& mesh)
{
	TriMeshPtr dest_mesh;
	trimesh::xform xf;
	size_t size = m_key_frames.size();
	if (size > 0)
	{
		std::vector<float> errors(size, -1.0f);
		std::vector<trimesh::xform> matrixes(size);
		Concurrency::parallel_for<size_t>(0, size, [this, &mesh, &matrixes, &errors](size_t i) {
			trimesh::ProjectionICP icp(m_fx, m_fy, m_cx, m_cy);
			icp.SetSource(mesh.get());
			icp.SetTarget(m_key_frames.at(i).get());
			errors.at(i) = icp.Do(matrixes.at(i));
		});

		float min_error = FLT_MAX;
		int index = -1;
		for (size_t i = 0; i < size; ++i)
		{
			float e = errors.at(i);
			if (e <= 0.1f && e >= 0.0f && e < min_error)
			{
				min_error = e;
				index = (int)i;
			}
		}

		if (index >= 0 && index < (int)size)
		{
			dest_mesh = m_key_frames.at(index);
			xf = matrixes.at(index);
		}
	}

	if (dest_mesh)
	{
		mesh->global = xf * dest_mesh->global;
		std::cout << mesh->frame << " relocate success. ("<<dest_mesh->frame<<") at "<< size <<"key frames"<< std::endl;
		return true;
	}
	else
	{
		return false;
	}
}

void VOImpl::SetLastMesh(TriMeshPtr& mesh, bool use_as_keyframe)
{
	m_last_mesh = mesh;
	m_last_mesh->clear_grid();

	if(use_as_keyframe) m_key_frames.push_back(mesh);
}