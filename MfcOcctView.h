// MfcOcctView.h: CMfcOcctView 类的接口
//

#pragma once

#include "AIS_ViewController.hxx"
#include "LOD\LodTrigger.h"
#include <AIS_TextLabel.hxx>
#include <Standard_Real.hxx>

class CloudLodController;
class SceneHud;

class CMfcOcctDoc;

enum CurAction3d
{
	CurAction3d_Nothing,
	CurAction3d_DynamicZooming,
	CurAction3d_WindowZooming,
	CurAction3d_DynamicPanning,
	CurAction3d_GlobalPanning,
	CurAction3d_DynamicRotation
};

class CMfcOcctView : public CView, public AIS_ViewController
{
protected: // 仅从序列化创建
	CMfcOcctView() noexcept;
	DECLARE_DYNCREATE(CMfcOcctView)

	Handle(AIS_InteractiveContext) myAisContext;
	Handle(V3d_Viewer)	myViewer;
	Handle(V3d_View)    myView;
	AIS_MouseGestureMap myDefaultGestures;
	Graphic3d_Vec2i     myClickPos;
	Standard_Real       myCurZoom;
	unsigned int        myUpdateRequests; //!< counter for unhandled update requests
	CurAction3d         myCurrentMode;

	LodTrigger m_lod;                       // LOD触发器
	std::unique_ptr<CloudLodController> m_lodCtl;
	std::unique_ptr<SceneHud> m_sceneHud;

	//! Handle view redraw.
	virtual void handleViewRedraw(const Handle(AIS_InteractiveContext)&,
		const Handle(V3d_View)& theView,
		Standard_Boolean bReDrawImmediate = Standard_True) Standard_OVERRIDE;

	//! Return interactive context.
	virtual const Handle(AIS_InteractiveContext)& GetAISContext() const { return myAisContext; }

	//! Setup mouse gestures.
	void defineMouseGestures();

	//! Get current action.
	CurAction3d getCurrentAction() const { return myCurrentMode; }

	//! Set current action.
	void setCurrentAction(CurAction3d theAction)
	{
		myCurrentMode = theAction;
		defineMouseGestures();
	}

	void InitTriedron();
	void InitLightSource();

	// 特性
public:
	CMfcOcctDoc* GetDocument() const;

	//! Return the view.
	const Handle(V3d_View)& GetView() const { return myView; }

	void FitAll() { if (!myView.IsNull()) myView->FitAll();  myView->ZFitAll(); };
	void Redraw() { if (!myView.IsNull()) myView->Redraw(); };

	void lodUpdateView();

	//! Request view redrawing.
	void update3dView();

	//! Flush events and redraw view.
	void redraw3dView();

	void UpdateHud();
	// 操作
public:

	// 重写
public:
	virtual void OnDraw(CDC* pDC);  // 重写以绘制该视图
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);

	// 实现
public:
	virtual ~CMfcOcctView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	virtual void OnInitialUpdate() Standard_OVERRIDE;
	virtual void PostNcDestroy() Standard_OVERRIDE;

	// 生成的消息映射函数
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnFilePrintPreview();
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnMouseWheel(UINT theFlags, short theDelta, CPoint thePoint);
	afx_msg void OnMouseMove(UINT theFlags, CPoint thePoint);
	afx_msg void OnMouseLeave();
	afx_msg void OnLButtonDown(UINT theFlags, CPoint thePoint);
	afx_msg void OnLButtonUp(UINT theFlags, CPoint thePoint);
	afx_msg void OnMButtonDown(UINT theFlags, CPoint thePoint);
	afx_msg void OnMButtonUp(UINT theFlags, CPoint thePoint);
	afx_msg void OnRButtonDown(UINT theFlags, CPoint thePoint);
	afx_msg void OnRButtonUp(UINT theFlags, CPoint thePoint);
public:
	afx_msg void OnImportTxtCloud();
	afx_msg void OnCreateCube();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnFitAll();
};

#ifndef _DEBUG  // MfcOcctView.cpp 中的调试版本
inline CMfcOcctDoc* CMfcOcctView::GetDocument() const
{
	return reinterpret_cast<CMfcOcctDoc*>(m_pDocument);
}
#endif
