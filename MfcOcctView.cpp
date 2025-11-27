// MfcOcctView.cpp: CMfcOcctView 类的实现
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS 可以在实现预览、缩略图和搜索筛选器句柄的
// ATL 项目中进行定义，并允许与该项目共享文档代码。
#ifndef SHARED_HANDLERS
#include "MfcOcct.h"
#endif

#include "MfcOcctDoc.h"
#include "MfcOcctView.h"
#include "LOD\CloudLodController.hxx"
#include "BRepPrimAPI_MakeBox.hxx"
#include "SceneHud.hxx"

// CMfcOcctView

IMPLEMENT_DYNCREATE(CMfcOcctView, CView)

BEGIN_MESSAGE_MAP(CMfcOcctView, CView)
	ON_WM_SIZE()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSEWHEEL()
	ON_WM_MOUSELEAVE()
	ON_WM_NCMOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	// 标准打印命令
	ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CMfcOcctView::OnFilePrintPreview)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_IMPORT_TXT_CLOUD, &CMfcOcctView::OnImportTxtCloud)
	ON_COMMAND(ID_CREATE_CUBE, &CMfcOcctView::OnCreateCube)
	ON_WM_TIMER()
	ON_COMMAND(ID_FIT_ALL, &CMfcOcctView::OnFitAll)
END_MESSAGE_MAP()

// CMfcOcctView 构造/析构

CMfcOcctView::CMfcOcctView() noexcept
	: myUpdateRequests(0),
	myCurZoom(0.0),
	myCurrentMode(CurAction3d_Nothing),
	myDefaultGestures(myMouseGestureMap)
{
	Handle(Graphic3d_GraphicDriver) aGraphicDriver = ((CMfcOcctApp*)AfxGetApp())->GetGraphicDriver();

	// create the Viewer
	myViewer = new V3d_Viewer(aGraphicDriver);
	myViewer->SetDefaultLights();
	myViewer->SetLightOn();
	myViewer->SetDefaultViewProj(V3d_Zpos);
	myViewer->SetDefaultShadingModel(Graphic3d_TOSM_FRAGMENT);

	// set default values for grids
	myViewer->SetCircularGridValues(0, 0, 10, 8, 0);
	myViewer->SetRectangularGridValues(0, 0, 10, 10, 0);

	myAisContext = new AIS_InteractiveContext(myViewer);
	myAisContext->SetDisplayMode(AIS_Shaded, Standard_True);
}

#include "AIS_ViewCube.hxx"
void CMfcOcctView::InitTriedron()
{
	//	生成视图盒子
	Handle(AIS_ViewCube) myViewCube = new AIS_ViewCube();
	static const double CUBE_SIZE = 80.0;
	static const double DEVICE_PIXEL_RATIO = 1;
	myViewCube->SetSize(DEVICE_PIXEL_RATIO * CUBE_SIZE, false);
	myViewCube->SetBoxFacetExtension(myViewCube->Size() * 0.15);
	myViewCube->SetAxesPadding(myViewCube->Size() * 0.10);
	myViewCube->SetFontHeight(CUBE_SIZE * 0.16);
	myViewCube->SetTransformPersistence(
		new Graphic3d_TransformPers(Graphic3d_TMF_TriedronPers, Aspect_TOTP_LEFT_LOWER, Graphic3d_Vec2i(CUBE_SIZE * 1.5, CUBE_SIZE * 1.5)));

	GetAISContext()->Display(myViewCube, Standard_True);
}

void CMfcOcctView::InitLightSource()
{
	const V3d_ListOfLight& activeLights = myViewer->ActiveLights();
	for (V3d_ListOfLight::Iterator lightIter(activeLights); lightIter.More(); lightIter.Next())
	{
		Handle(Graphic3d_CLight)& light = lightIter.Value();

		switch (light->Type())
		{
		case Graphic3d_TOLS_AMBIENT:
			light->SetColor(Quantity_Color(0.06, 0.06, 0.06, Quantity_TOC_RGB));
			break;
		case Graphic3d_TOLS_DIRECTIONAL:
			light->SetColor(Quantity_Color(0.8, 0.8, 0.8, Quantity_TOC_RGB));
			light->SetDirection(0, -0.707, -0.707);
			break;
		default:
			break;
		}
	}

	Handle(Graphic3d_CLight) light3 = new Graphic3d_CLight(Graphic3d_TOLS_DIRECTIONAL);
	light3->SetColor(Quantity_Color(0.9, 0.9, 0.9, Quantity_TOC_RGB));
	light3->SetDirection(-0.5, -0.5, -2.5);
	light3->SetEnabled(Standard_True);
	myViewer->AddLight(light3);
	myViewer->SetLightOn(light3);

	Handle(Graphic3d_CLight) light4 = new Graphic3d_CLight(Graphic3d_TOLS_DIRECTIONAL);
	light4->SetColor(Quantity_Color(0.8, 0.8, 0.8, Quantity_TOC_RGB));
	light4->SetDirection(0, 0, -0.5);
	light4->SetEnabled(Standard_True);
	light4->SetHeadlight(Standard_True);
	myViewer->AddLight(light4);
	myViewer->SetLightOn(light4);
}

CMfcOcctView::~CMfcOcctView()
{
}
void CMfcOcctView::PostNcDestroy()
{
	if (!myView.IsNull())
	{
		myView->Remove();
		myView.Nullify();
	}
	CView::PostNcDestroy();
}

BOOL CMfcOcctView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: 在此处通过修改
	//  CREATESTRUCT cs 来修改窗口类或样式

	return CView::PreCreateWindow(cs);
}

// CMfcOcctView 打印

void CMfcOcctView::OnFilePrintPreview()
{
#ifndef SHARED_HANDLERS
	AFXPrintPreview(this);
#endif
}

BOOL CMfcOcctView::OnPreparePrinting(CPrintInfo* pInfo)
{
	// 默认准备
	return DoPreparePrinting(pInfo);
}

void CMfcOcctView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: 添加额外的打印前进行的初始化过程
}

void CMfcOcctView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: 添加打印后进行的清理过程
}

void CMfcOcctView::OnContextMenu(CWnd* /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}

// CMfcOcctView 诊断

#ifdef _DEBUG
void CMfcOcctView::AssertValid() const
{
	CView::AssertValid();
}

void CMfcOcctView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CMfcOcctDoc* CMfcOcctView::GetDocument() const // 非调试版本是内联的
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CMfcOcctDoc)));
	return (CMfcOcctDoc*)m_pDocument;
}
#endif //_DEBUG

// CMfcOcctView 消息处理程序

void CMfcOcctView::OnInitialUpdate()
{
	__super::OnInitialUpdate();

	myCurrentMode = CurAction3d_Nothing;
	CView::OnInitialUpdate();
	if (!myView.IsNull())
	{
		return;
	}

	myView = GetAISContext()->CurrentViewer()->CreateView();
	myView->SetImmediateUpdate(false);
	myView->SetComputedMode(Standard_False);

	Handle(OpenGl_GraphicDriver) aDriver = Handle(OpenGl_GraphicDriver)::DownCast(myView->Viewer()->Driver());
	myView->Camera()->SetProjectionType(aDriver->Options().contextStereo
		? Graphic3d_Camera::Projection_Stereo
		: Graphic3d_Camera::Projection_Orthographic);

	Handle(WNT_Window) aWNTWindow = new WNT_Window(GetSafeHwnd());
	myView->SetWindow(aWNTWindow);
	if (!aWNTWindow->IsMapped()) aWNTWindow->Map();

	Quantity_Color color[2] = {
		{10 / 255.0, 18 / 255.0, 26 / 255.0, Quantity_TOC_RGB},
		{11 / 255.0, 19 / 255.0, 28 / 255.0, Quantity_TOC_RGB}
	};
	myView->SetBgGradientColors(color[0], color[1], Aspect_GradientFillMethod_Horizontal, Standard_True);

	m_lodCtl = std::make_unique<CloudLodController>(myAisContext, myView);
	m_lod.timerId = 1001;   // 自定
	m_lod.debounceMs = 150;    // 可调

	if (!myAisContext.IsNull() && !myView.IsNull()) {
		m_sceneHud = std::make_unique<SceneHud>(myAisContext, myView);
	}

	InitTriedron();
	InitLightSource();

	myView->Redraw();
	myView->Invalidate();
}

// ================================================================
// Function : update3dView
// Purpose  :
// ================================================================
void CMfcOcctView::update3dView()
{
	if (!myView.IsNull())
	{
		if (++myUpdateRequests == 1)
		{
			Invalidate(FALSE);
			UpdateWindow();
		}
	}
}

// ================================================================
// Function : redraw3dView
// Purpose  :
// ================================================================
void CMfcOcctView::redraw3dView()
{
	if (!myView.IsNull())
	{
		FlushViewEvents(GetAISContext(), myView, true);
	}
}

// ================================================================
// Function : handleViewRedraw
// Purpose  :
// ================================================================
void CMfcOcctView::handleViewRedraw(const Handle(AIS_InteractiveContext)& theCtx,
	const Handle(V3d_View)& theView,
	Standard_Boolean bReDrawImmediate)
{
	myUpdateRequests = 0;
	AIS_ViewController::handleViewRedraw(theCtx, theView);
}

// =======================================================================
// function : OnDraw
// purpose  :
// =======================================================================
void CMfcOcctView::OnDraw(CDC*)
{
	if (myView.IsNull())
		return;

	// always redraw immediate layer (dynamic highlighting) on Paint event,
	// and redraw entire view content only when it is explicitly invalidated (V3d_View::Invalidate())
	myView->InvalidateImmediate();
	FlushViewEvents(GetAISContext(), myView, true);
}

// =======================================================================
// function : defineMouseGestures
// purpose  :
// =======================================================================
void CMfcOcctView::defineMouseGestures()
{
	myMouseGestureMap.Clear();
	AIS_MouseGesture aRot = AIS_MouseGesture_RotateOrbit;
	switch (myCurrentMode)
	{
	case CurAction3d_Nothing:
	{
		myMouseGestureMap = myDefaultGestures;
		break;
	}
	case CurAction3d_DynamicZooming:
	{
		myMouseGestureMap.Bind(Aspect_VKeyMouse_LeftButton, AIS_MouseGesture_Zoom);
		break;
	}
	case CurAction3d_GlobalPanning:
	{
		break;
	}
	case CurAction3d_WindowZooming:
	{
		myMouseGestureMap.Bind(Aspect_VKeyMouse_LeftButton, AIS_MouseGesture_ZoomWindow);
		break;
	}
	case CurAction3d_DynamicPanning:
	{
		myMouseGestureMap.Bind(Aspect_VKeyMouse_LeftButton, AIS_MouseGesture_Pan);
		break;
	}
	case CurAction3d_DynamicRotation:
	{
		myMouseGestureMap.Bind(Aspect_VKeyMouse_LeftButton, aRot);
		break;
	}
	}
}

// =======================================================================
// function : OnMouseMove
// purpose  :
// =======================================================================
void CMfcOcctView::OnMouseMove(UINT theFlags, CPoint thePoint)
{
	TRACKMOUSEEVENT aMouseEvent;          // for WM_MOUSELEAVE
	aMouseEvent.cbSize = sizeof(aMouseEvent);
	aMouseEvent.dwFlags = TME_LEAVE;
	aMouseEvent.hwndTrack = m_hWnd;
	aMouseEvent.dwHoverTime = HOVER_DEFAULT;
	if (!::_TrackMouseEvent(&aMouseEvent)) { TRACE("Track ERROR!\n"); }

	const Aspect_VKeyFlags aFlags = WNT_Window::MouseKeyFlagsFromEvent(theFlags);
	if (UpdateMousePosition(Graphic3d_Vec2i(thePoint.x, thePoint.y), PressedMouseButtons(), aFlags, false))
	{
		m_lod.Mark(m_hWnd);
		update3dView();
	}
}

// =======================================================================
// function : OnLButtonDown
// purpose  :
// =======================================================================
void CMfcOcctView::OnLButtonDown(UINT theFlags, CPoint thePoint)
{
	const Aspect_VKeyFlags aFlags = WNT_Window::MouseKeyFlagsFromEvent(theFlags);
	PressMouseButton(Graphic3d_Vec2i(thePoint.x, thePoint.y), Aspect_VKeyMouse_LeftButton, aFlags, false);
	update3dView();
}

// =======================================================================
// function : OnLButtonUp
// purpose  :
// =======================================================================
void CMfcOcctView::OnLButtonUp(UINT theFlags, CPoint thePoint)
{
	const Aspect_VKeyFlags aFlags = WNT_Window::MouseKeyFlagsFromEvent(theFlags);
	ReleaseMouseButton(Graphic3d_Vec2i(thePoint.x, thePoint.y), Aspect_VKeyMouse_LeftButton, aFlags, false);
	if (myCurrentMode == CurAction3d_GlobalPanning)
	{
		myView->Place(thePoint.x, thePoint.y, myCurZoom);
		myView->Invalidate();
	}
	if (myCurrentMode != CurAction3d_Nothing)
	{
		setCurrentAction(CurAction3d_Nothing);
	}
	update3dView();
}

// =======================================================================
// function : OnMButtonDown
// purpose  :
// =======================================================================
void CMfcOcctView::OnMButtonDown(UINT theFlags, CPoint thePoint)
{
	const Aspect_VKeyFlags aFlags = WNT_Window::MouseKeyFlagsFromEvent(theFlags);
	PressMouseButton(Graphic3d_Vec2i(thePoint.x, thePoint.y), Aspect_VKeyMouse_MiddleButton, aFlags, false);
	update3dView();
}

// =======================================================================
// function : OnMButtonUp
// purpose  :
// =======================================================================
void CMfcOcctView::OnMButtonUp(UINT theFlags, CPoint thePoint)
{
	const Aspect_VKeyFlags aFlags = WNT_Window::MouseKeyFlagsFromEvent(theFlags);
	ReleaseMouseButton(Graphic3d_Vec2i(thePoint.x, thePoint.y), Aspect_VKeyMouse_MiddleButton, aFlags, false);
	update3dView();
	if (myCurrentMode != CurAction3d_Nothing)
	{
		setCurrentAction(CurAction3d_Nothing);
	}
}

// =======================================================================
// function : OnRButtonDown
// purpose  :
// =======================================================================
void CMfcOcctView::OnRButtonDown(UINT theFlags, CPoint thePoint)
{
	const Aspect_VKeyFlags aFlags = WNT_Window::MouseKeyFlagsFromEvent(theFlags);
	PressMouseButton(Graphic3d_Vec2i(thePoint.x, thePoint.y), Aspect_VKeyMouse_RightButton, aFlags, false);
	update3dView();
	myClickPos.SetValues(thePoint.x, thePoint.y);
}

// =======================================================================
// function : OnRButtonUp
// purpose  :
// =======================================================================
void CMfcOcctView::OnRButtonUp(UINT theFlags, CPoint thePoint)
{
	const Aspect_VKeyFlags aFlags = WNT_Window::MouseKeyFlagsFromEvent(theFlags);
	ReleaseMouseButton(Graphic3d_Vec2i(thePoint.x, thePoint.y), Aspect_VKeyMouse_RightButton, aFlags, false);
	update3dView();
	if (myCurrentMode != CurAction3d_Nothing)
	{
		setCurrentAction(CurAction3d_Nothing);
	}
	if (aFlags == Aspect_VKeyFlags_NONE
		&& (myClickPos - Graphic3d_Vec2i(thePoint.x, thePoint.y)).cwiseAbs().maxComp() <= 4)
	{
		//GetDocument()->Popup(thePoint.x, thePoint.y, myView);
	}
}

// =======================================================================
// function : OnMouseWheel
// purpose  :
// =======================================================================
BOOL CMfcOcctView::OnMouseWheel(UINT theFlags, short theDelta, CPoint thePoint)
{
	const Standard_Real aDeltaF = Standard_Real(theDelta) / Standard_Real(WHEEL_DELTA);
	CPoint aCursorPnt = thePoint;
	ScreenToClient(&aCursorPnt);
	const Graphic3d_Vec2i  aPos(aCursorPnt.x, aCursorPnt.y);
	const Aspect_VKeyFlags aFlags = WNT_Window::MouseKeyFlagsFromEvent(theFlags);
	if (UpdateMouseScroll(Aspect_ScrollDelta(aPos, aDeltaF, aFlags)))
	{
		m_lod.Mark(m_hWnd);
		update3dView();
	}
	return true;
}

// =======================================================================
// function : OnSize
// purpose  :
// =======================================================================
void CMfcOcctView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	if (cx != 0
		&& cy != 0
		&& !myView.IsNull())
	{
		m_lod.Mark(m_hWnd);

		myView->Window()->DoResize();
		myView->MustBeResized();
		myView->Invalidate();

		update3dView();
	}
}

// =======================================================================
// function : OnMouseLeave
// purpose  :
// =======================================================================
void CMfcOcctView::OnMouseLeave()
{
	CPoint aCursorPos;
	if (GetCursorPos(&aCursorPos))
	{
		ReleaseMouseButton(Graphic3d_Vec2i(aCursorPos.x, aCursorPos.y),
			PressedMouseButtons(),
			Aspect_VKeyMouse_NONE,
			false);
	}
}

#include "CloudDataStore.hxx"
#include "AIS_Cloud.hxx"
#include "Geom_RectangularTrimmedSurface.hxx"
void CMfcOcctView::OnImportTxtCloud()
{
	CFileDialog dlg(FALSE, L"txt", L"", OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, L"TXT Files (*.txt)|*.txt||");
	if (dlg.DoModal() != IDOK)
	{
		return;
	}

	// 获取用户选择的文件路径
	CString filePath = dlg.GetPathName();

	auto store = std::make_shared<CloudDataStore>();

	// 调用 LoadTxtXYZMapped 加载点云数据
	// 假设文件每行有 6 列：x y z nx ny nz，XYZ 分别在第 0/1/2 列
	if (!store->LoadTxtMappedAuto(std::wstring(filePath)))
	{
		AfxMessageBox(L"加载点云数据失败，请检查文件格式是否正确。");
		return;
	}

	// 然后交给 AIS_Cloud 使用
	Handle(AIS_Cloud) cloud = new AIS_Cloud();
	cloud->SetDataStore(store);
	cloud->SetView(myView);

	m_lodCtl->RegisterCloud(cloud);
	m_lodCtl->Tick();

	myAisContext->Display(cloud, Standard_False);
	myAisContext->UpdateCurrentViewer();
	myView->FitAll();
}

void CMfcOcctView::OnCreateCube()
{
	// 定义长方体的尺寸
	Standard_Real length = 100.0; // 长
	Standard_Real width = 50.0;   // 宽
	Standard_Real height = 75.0; // 高

	// 使用 BRepPrimAPI_MakeBox 创建长方体
	TopoDS_Shape aBox = BRepPrimAPI_MakeBox(length, width, height);

	// 创建 AIS_Shape 用于显示
	Handle(AIS_Shape) aShape = new AIS_Shape(aBox);

	// 设置显示属性（可选）
	aShape->SetColor(Quantity_NOC_BLUE1); // 设置颜色为蓝色

	// 在 AIS_InteractiveContext 中显示长方体
	myAisContext->Display(aShape, Standard_True);

	// 调整视图以适应新添加的对象
	myView->FitAll();
}

void CMfcOcctView::OnTimer(UINT_PTR nIDEvent)
{
	if (m_lod.OnTimer(m_hWnd, nIDEvent)) {
		// 通过防抖，确认视图期间发生过变化
		bool anyChanged = false;
		if (m_lodCtl) {
			anyChanged = m_lodCtl->Tick();   // 计算 LOD、标记 AIS_Cloud SetToUpdate
		}

		if (anyChanged)
		{
			m_lodCtl->UpdateDisplayedStats();

			// 把 RuntimeStats + DisplayStats 都刷到 HUD
			UpdateHud();

			myView->Redraw();
		}
	}
	__super::OnTimer(nIDEvent);
}

void CMfcOcctView::OnFitAll()
{
	myView->FitAll();
}

void CMfcOcctView::UpdateHud()
{
	if (!m_sceneHud || !m_lodCtl)
		return;

	const auto& hs = m_lodCtl->HudStatistics();

	TCollection_AsciiString txt;

	txt += "LOD Mode: ";
	txt += (m_lodCtl->UseExperimental() ? "EXPERIMENT" : "BASELINE");
	txt += "\n";

	txt += "Cloud points (total): ";
	txt += (Standard_Integer)hs.globalPoints;
	txt += "\n";

	txt += "Budget (points): ";
	txt += hs.budget;
	txt += "\n";

	txt += "LOD chosen points: ";
	txt += (Standard_Integer)hs.pointsChosen;
	txt += "\n";

	txt += "LOD chosen tiles: ";
	txt += (Standard_Integer)hs.nodesShown;
	txt += "\n";

	txt += "Displayed tiles: ";
	txt += hs.displayedTiles;
	txt += "\n";

	txt += "Displayed points: ";
	txt += hs.displayedPoints;
	txt += "\n";

	txt += "selectLOD time (ms): ";
	txt += hs.selectMs;

	m_sceneHud->Update(txt);
}