#include "render.h"
#include <base\bind.h>

Render::Render()
	:base::Thread("Render")
{
	m_scene = new SimulationScene();
}

Render::~Render()
{

}

void Render::OnFrame(esslam::PatchRenderData* data)
{
	task_runner()->PostTask(FROM_HERE, base::Bind(&Render::ShowPatch, base::Unretained(this), data));
}

void Render::StartRender()
{
	Start();
}

void Render::StopRender()
{
	Stop();
}

SimulationScene* Render::GetScene()
{
	return m_scene;
}

void Render::Convert(osg::Matrixf& matrix, float* xf)
{
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			matrix(i, j) = xf[4 * i + j];
}

void Render::ShowPatch(esslam::PatchRenderData* data)
{
	BuildPatch(data);
	osg::Matrixf matrix = osg::Matrixf::identity();
	Convert(matrix, data->xf);
	m_scene->UpdateMatrix(matrix, data->step == 0);
}

void Render::BuildPatch(esslam::PatchRenderData* data)
{
	m_scene->Lock();
	m_scene->UpdateCountText((int)data->indices.size());
	size_t exsit_size = m_geometries.size();
	std::vector<int> flags;
	if (exsit_size > 0) flags.resize(exsit_size, 0); //0: nothing,  1: update, 2: add

	std::vector<osg::ref_ptr<ChunkGeometry>> new_geometries;

	const std::vector<int> idxs = data->indices;
	const std::vector<float>& points = data->points;
	const std::vector<float>& norms = data->normals;
	size_t input_size = idxs.size();
	bool has_color = false;
	float t = m_scene->GetTime();
	for (size_t i = 0; i < input_size; ++i)
	{
		int index = idxs.at(i);
		if (index < 0) continue;
		const float* tp = &points.at(3 * i);
		const float* tn = &norms.at(3 * i);

		osg::Vec3f p = osg::Vec3f(tp[0], tp[1], tp[2]);
		osg::Vec3f n = osg::Vec3f(tn[0], tn[1], tn[2]);
		osg::Vec4f c = osg::Vec4f(0.8f, 0.8f, 0.8f, 1.0f);

		int chunk = index / ChunkVertexSize;
		int rindex = index % ChunkVertexSize;

		while (chunk >= (int)m_geometries.size())
		{
			ChunkGeometry* geometry = new ChunkGeometry();
			m_geometries.push_back(geometry);
			new_geometries.push_back(geometry);
		}

		ChunkGeometry* geometry = m_geometries.at(chunk);
		geometry->Update(rindex, p, n, c, t);
	}

	for (size_t i = 0; i < m_geometries.size(); ++i)
	{
		m_geometries.at(i)->Check();
	}

	for (size_t i = 0; i < new_geometries.size(); ++i)
		m_scene->AddPatch(new_geometries.at(i));

	bool first_frame = exsit_size == 0;
	if(first_frame) m_scene->UpdateCam();

	m_scene->Unlock();
}