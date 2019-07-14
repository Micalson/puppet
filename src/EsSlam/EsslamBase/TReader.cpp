#include "TReader.h"
#include "base/bind.h"
#include "..//interface/slam_tracer.h"
#include "DFrame.h"

namespace esslam
{
	TReader::TReader()
		:base::Thread("TReader"), m_delta_time(0.01f)
	{

	}

	TReader::~TReader()
	{

	}

	void TReader::StartInput(const SlamParameters& parameters)
	{
		bool start = Start();

		const ReaderParameters& read_param = parameters.reader_param;
		m_delta_time = read_param.time;

		const ImageParameters& image_param = parameters.image_param;
		m_dframe_pool.Melloc(3, image_param.width, image_param.height);

		if (start && read_param.load_from_file)
		{
			m_reader.SetParameters(read_param.directory, read_param.pattern);
			m_last_time = trimesh::now();
			task_runner()->PostTask(FROM_HERE, base::Bind(&TReader::Read, base::Unretained(this)));
		}
	}

	void TReader::StopInput()
	{
		Stop();
	}

	void TReader::SetImageData(HandleScanImageData* data)
	{

	}

	void TReader::Release(DFrame* frame)
	{
		m_dframe_pool.Release(frame);
	}

	void TReader::Read()
	{
		DFrame* frame = m_dframe_pool.Get();
		bool result = m_reader.Load(*frame);
		if (result)
		{
			trimesh::timestamp now_time = trimesh::now();
			float dt = now_time - m_last_time;
			m_last_time = now_time;
			if (dt < m_delta_time && dt > 0.0f)
				::Sleep(DWORD(1000.0f * (m_delta_time - dt)));

			m_processor->ProcessFrame(frame);
			task_runner()->PostTask(FROM_HERE, base::Bind(&TReader::Read, base::Unretained(this)));
		}
		else
		{
			m_dframe_pool.Release(frame);
			if (m_read_tracer) m_read_tracer->OnFinished();
		}
	}
}