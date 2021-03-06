#pragma once
#include "../interface/slam_interface.h"
#include "InnerInterface.h"
#include "projectionicp.h"
#include "VoState.h"
#include "Octree.h"
#include "SlamParameters.h"

namespace esslam
{

	class ESSLAM_API VOImpl
	{
	public:
		VOImpl();
		~VOImpl();

		void Setup(const SlamParameters& parameters);
		void SetVisualProcessor(VisualProcessor* processor);
		void SetLocateTracer(LocateTracer* tracer);
		void SetVOProfiler(VOProfiler* profiler);
		void SetProjectionICPTracer(trimesh::ProjectionICPTracer* tracer);

		void ProcessOneFrame(TriMeshPtr& mesh, LocateData& locate_data);

		void Clear();
		void Build(IBuildTracer& tracer);
		void SetFixMode();
	protected:
		void LocateOneFrame(TriMeshPtr& mesh, LocateData& locate_data);
		void FusionFrame(TriMeshPtr& mesh, const LocateData& locate_data);

		bool Frame2Frame(TriMeshPtr& mesh);
		bool Frame2Model(TriMeshPtr& mesh, bool relocate = false);
		bool Relocate(TriMeshPtr& mesh);
		void SetLastMesh(TriMeshPtr& mesh, bool use_as_keyframe);

		void ProcessOneFrameFix(TriMeshPtr& mesh, LocateData& locate_data);
	protected:
		ICPParamters m_icp_parameters;

		VisualProcessor* m_visual_processor;
		TriMeshPtr m_last_mesh;
		std::unique_ptr<trimesh::ProjectionICP> m_icp;

		std::vector<TriMeshPtr> m_key_frames;

		float m_fx;
		float m_fy;
		float m_cx;
		float m_cy;

		VOState m_state;

		std::unique_ptr<Octree> m_octree;
		std::vector<int> m_layers;

		bool m_use_fast_icp;
		LocateTracer* m_locate_tracer;
		trimesh::ProjectionICPTracer* m_icp_tracer;
		VOProfiler* m_profiler;

		std::vector<trimesh::xform> m_xforms;
		bool m_fix_mode;
	};
}