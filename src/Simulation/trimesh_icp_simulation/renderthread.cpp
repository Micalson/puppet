#include "renderthread.h"
#include <base/bind.h>

RenderThreadBaseScene::RenderThreadBaseScene()
{

}

RenderThreadBaseScene::~RenderThreadBaseScene()
{

}

RenderThread::RenderThread(base::WaitableEvent& e)
	:base::Thread("RenderThread"), m_exit_event(e)
{

}

RenderThread::~RenderThread()
{

}

void RenderThread::StartRender(RenderThreadBaseScene* scene)
{
	m_scene = scene;

	bool start = Start();
	if (start) task_runner()->PostTask(FROM_HERE, base::Bind(&RenderThread::RenderCircle, base::Unretained(this)));
}

void RenderThread::StopRender()
{
	Stop();
}

void RenderThread::RenderCircle()
{
	m_view = new OSGWrapper::RenderView();
	m_view->SetBackgroundColor(osg::Vec4(0.3f, 0.3f, 0.3f, 1.0f));
	m_view->setUpViewInWindow(0, 0, 1920, 1080);
	m_view->SetCurrentScene(m_scene);

	m_viewer.addView(m_view);
	//m_viewer.setKeyEventSetsDone(0);
	m_viewer.setReleaseContextAtEndOfFrameHint(false);

	double last_frame_time = 0.0f;
	while (!m_viewer.done())
	{
		if(m_scene)
		{
			m_scene->render_lock.Acquire();
			m_viewer.frame();
			m_scene->render_lock.Release();
		}
		else
			m_viewer.frame();

		double time = m_viewer.getFrameStamp()->getReferenceTime();
		double dt = time - last_frame_time;
		if (dt <= 0.03)
			::Sleep(30 - (DWORD)(dt * 1000.0));
		last_frame_time = time;
	}

	m_view->SetCurrentScene(NULL);
	
	m_viewer.removeView(m_view);
	m_view = NULL;

	m_exit_event.Signal();
}