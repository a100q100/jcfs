// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MainDlg.h"
	
#ifdef DWMBLUR	//win7毛玻璃开关
#include <dwmapi.h>
#pragma comment(lib,"dwmapi.lib")
#endif
#include "FsMcAdapterFix.h"
#include "ShellContextMenu.h"
#include "resource.h"
#include <util/vercmp.h>
#include <shellapi.h>
#include <thread/threadex.h>
#include <network/HttpImplement.h>
#include <msapi/msapp.h>
#include <json/json.h>
#include <msapi/msClipboard.h>
#include <msapi/mssrv.h>
#include <process/ParseCommand.h>

#include <Commctrl.h>
#include "SDlgSrvError.h"
#include "SFreeImageWnd.h"
#include "SShowImageWnd.h"
#include "SShowGifWnd.h"
//结果列表菜单



//#include "PathEdit.h"
enum enumListColumn
{
	//LC_Stat_Icon = 0,			   //icon
	LC_Stat_Name,				   //名称
	LC_Stat_Path,				   //路劲
	LC_Stat_Size,				   //大小
	//LC_Stat_Extension,             //扩展名
	//LC_Stat_CreationTime,		   //创建时间
	LC_Stat_LastAccessTime,        //最后访问时间

	LC_Null = 999,
};

struct ListColumnDef
{
	enumListColumn listColumn;
	LPCTSTR        strText;
	INT			   nWidth;

	UINT		   mask;
	INT			   Fmt;
};


/* list
WS_CHILDWINDOW
WS_VISIBLE
WS_CLIPSIBLINGS
WS_CLIPCHILDREN
WS_VSCROLL
WS_HSCROLL
WS_MAXIMIZEBOX


WS_EX_LEFT
WS_EX_LTRREADING
WS_EX_RIGHTSCROLLBAR
WS_EX_CLIENTEDGE
*/
/* head 

n:
WS_CHILDWINDOW
WS_VISIBLE
HDS_HORZ
HDS_BUTTONS
HDS_DRAGDROP
HDS_FULLDRAG

ex:
WS_EX_LEFT
WS_EX_LTRREADING
WS_EX_RIGHTSCROLLBAR


*/
static ListColumnDef g_ListColumnDef[] =
{
	//{ LC_Stat_Icon, L"", 0, 0 },
	{ LC_Stat_Name, L"名称", 240 , HDI_FILTER | LVCF_TEXT| LVCF_WIDTH | LVCF_FMT,   HDF_SORTDOWN | HDF_IMAGE },
	{ LC_Stat_Path, L"路径", 300 ,LVCF_TEXT | LVCF_WIDTH | LVCF_FMT,LVCFMT_LEFT /*| HDF_SORTDOWN*/ |HDF_BITMAP_ON_RIGHT},
	{ LC_Stat_Size, L"大小", 70 , LVCF_TEXT | LVCF_WIDTH | LVCF_FMT,LVCFMT_LEFT},
	//{ LC_Stat_Extension, L"扩展名", 50 },
	//{ LC_Stat_CreationTime, L"创建时间", 105 },
	{ LC_Stat_LastAccessTime, L"最后访问时间", 115 ,LVCF_TEXT | LVCF_WIDTH | LVCF_FMT, LVCFMT_LEFT},

	{ LC_Null, NULL, 0 },
};

TCHAR *g_CtrlMap[] = { 
	_T("opt_mark_w") ,
	_T("opt_mark_hui") ,
	_T("opt_mark_z") ,
	_T("opt_mark_lan") ,
	_T("opt_mark_l") ,
	_T("opt_mark_q") ,
	_T("opt_mark_c") ,
	_T("opt_mark_h") ,
};

CMainDlg::CMainDlg(IMscomRunningObjectTable* pRot) : SHostWnd(_T("LAYOUT:XML_MAINWND"))
{
	m_bLayoutInited = FALSE;
	m_pTableCtrl = NULL;
	m_pRot = pRot;
	m_dwCurResultIndex = 0;
	ZeroMemory( &m_CurResult, sizeof(m_CurResult));

	CDiskSearchNotify::AddRef();

}

CMainDlg::~CMainDlg()
{
	
}

int CMainDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	#ifdef DWMBLUR	//win7毛玻璃开关
	MARGINS mar = {5,5,30,5};
	DwmExtendFrameIntoClientArea ( m_hWnd, &mar );
	#endif

	SetIcon(IDI_ICON_MAIN);
	
	SetMsgHandled(FALSE);

	return 0;
}

void CMainDlg::SetIcon(UINT nRes)
{
	HICON hIcon = (HICON)::LoadImage(g_hinstance, MAKEINTRESOURCE(nRes), IMAGE_ICON,
		(::GetSystemMetrics(SM_CXICON) + 15) & ~15, (::GetSystemMetrics(SM_CYICON) + 15) & ~15,	// 防止高DPI下图标模糊
		LR_DEFAULTCOLOR);
	
	::SendMessage(m_hWnd, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);


	hIcon = (HICON)::LoadImage(g_hinstance, MAKEINTRESOURCE(nRes), IMAGE_ICON,
		(::GetSystemMetrics(SM_CXICON) + 15) & ~15, (::GetSystemMetrics(SM_CYICON) + 15) & ~15,	// 防止高DPI下图标模糊
		LR_DEFAULTCOLOR);

	::SendMessage(m_hWnd, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);
}

BOOL CMainDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_bLayoutInited = TRUE;
	m_pTableCtrl = FindChildByName2<STabCtrl>(L"table_ctrl");
	m_pStatusCtrl = FindChildByName2<SStatic>(L"status_text");
	m_pListView = FindChildByName2<CWTLVirtualList>(_T("result_view"));
	SPanel* pPanel_1 = FindChildByName2<SPanel>(L"skin_img_ztl_bk");
	SPanel* pPanel = FindChildByName2<SPanel>(_T("check_updata"));


	DWORD dwStyle= ::GetWindowLong(m_pListView->m_hWnd,GWL_STYLE);
	::SetWindowLong(m_pListView->m_hWnd,GWL_STYLE,
		dwStyle
		|LVS_ICON
		|LVS_SINGLESEL);
	

 	dwStyle=ListView_GetExtendedListViewStyle(m_pListView->m_hWnd);
	dwStyle |= WS_EX_LEFT
			| LVS_EX_FULLROWSELECT
			| LVS_EX_DOUBLEBUFFER;

	ListView_SetExtendedListViewStyle(m_pListView->m_hWnd,dwStyle);

	for (int nIndex = 0; g_ListColumnDef[nIndex].strText != NULL; nIndex++)
	{
		m_pListView->DeleteColumn(nIndex);

		LV_COLUMN lvCol = {0};

		lvCol.mask = g_ListColumnDef[nIndex].mask;
		lvCol.fmt = g_ListColumnDef[nIndex].Fmt;
		lvCol.pszText = (LPWSTR)g_ListColumnDef[nIndex].strText;
		lvCol.cx = g_ListColumnDef[nIndex].nWidth;
	
		ListView_InsertColumn(m_pListView->m_hWnd,g_ListColumnDef[nIndex].listColumn,&lvCol);
	}


	SHFILEINFO shfi;
	ZeroMemory(&shfi,sizeof(SHFILEINFO));
	TCHAR dir[MAX_PATH];
	GetSystemDirectory(dir,MAX_PATH);
	dir[3]=0;

	HIMAGELIST hImageList=(HIMAGELIST)SHGetFileInfo(dir,0,&shfi,sizeof(SHFILEINFO),/*SHGFI_ICON |*/SHGFI_SYSICONINDEX|/*SHGFI_LARGEICON*/SHGFI_SMALLICON);
	ListView_SetImageList(m_pListView->m_hWnd,hImageList,LVSIL_SMALL);



	SEdit *pEditUrl = FindChildByName2<SEdit>(L"path_input");
	if(pEditUrl)
	{
		pEditUrl->SSendMessage(EM_SETEVENTMASK,0,ENM_CHANGE);

		//设置搜索目录
		CParseCommand ParseCmd(TRUE);
		if ( ParseCmd.IsExist(_T("filter")))
		{
			LPCTSTR lpszFilter = ParseCmd.GetArg(_T("filter"));
			pEditUrl->SetWindowText(lpszFilter);
		}
	}
	
	pEditUrl = FindChildByName2<SEdit>(L"search_input");
	if(pEditUrl)
		pEditUrl->SSendMessage(EM_SETEVENTMASK,0,ENM_CHANGE);

	
	
	
	
	m_pRot->CreateInstance(CLSID_RpcDiskSearchCli, NULL, re_uuidof(IDiskSearchCli), (void**)&m_pDiskSearch);
	if ( !m_pDiskSearch )
		return FALSE;

	if ( FAILED( m_pDiskSearch->Init( UTIL::com_ptr<IDiskSearchNotify>((CDiskSearchNotify*)this)) ))
	{
		return FALSE;
	}
	
	
	
	Post([=]
	{
		OnSearch();
	});
	
	
	
	
	CThreadEx().Do([this]{OnWorkCheckUpdata();});

	

	return TRUE;
}


void CMainDlg::OnDestory()
{
	

	MSG msg;
	while (PeekMessage(&msg, m_hWnd, WM_EXECUTE_MSG, WM_EXECUTE_MSG, PM_REMOVE)) 
	{
		DispatchMessage(&msg);
	}

	if ( m_pDiskSearch )
	{
		m_pDiskSearch->UnInit();
		m_pDiskSearch = INULL;
	}

	SAFE_RELEASE(m_pRot);
	SAFE_RELEASE(m_pResult);

	SetMsgHandled(FALSE);
}



void CMainDlg::Post(const std::function<void()> &f)
{
	auto func = new std::function<void()>(f);
	PostMessage( WM_EXECUTE_MSG, 0, (LPARAM)func);
}

VOID CMainDlg::SetTabIndex(int nIndex)
{
	if ( m_pTableCtrl )
	{
		if ( m_pTableCtrl->GetCurSel() != nIndex )
		{
			m_pTableCtrl->SetCurSel(nIndex);
		}
	}
}

//TODO:消息映射
void CMainDlg::OnClose()
{
	CSimpleWnd::DestroyWindow();
}

void CMainDlg::OnMaximize()
{
	SendMessage(WM_SYSCOMMAND, SC_MAXIMIZE);
}
void CMainDlg::OnRestore()
{
	SendMessage(WM_SYSCOMMAND, SC_RESTORE);
}
void CMainDlg::OnMinimize()
{
	SendMessage(WM_SYSCOMMAND, SC_MINIMIZE);
}

void CMainDlg::OnSize(UINT nType, SOUI::CSize size)
{
	SetMsgHandled(FALSE);
	if (!m_bLayoutInited) return;
	
	SWindow *pBtnMax = FindChildByName(L"btn_max");
	SWindow *pBtnRestore = FindChildByName(L"btn_restore");
	if(!pBtnMax || !pBtnRestore) return;
	
	if (nType == SIZE_MAXIMIZED)
	{
		pBtnRestore->SetVisible(TRUE);
		pBtnMax->SetVisible(FALSE);
	}
	else if (nType == SIZE_RESTORED)
	{
		pBtnRestore->SetVisible(FALSE);
		pBtnMax->SetVisible(TRUE);
	}
}

STDMETHODIMP CMainDlg::OnDiskSearch_FileChange(WCHAR cDosName,LPCWSTR lpFile, DWORD dwAction, DWORD dwAttr)
{
	CFuncTime funcTime(_T("CMainDlg："), _T("OnDiskSearch_FileChange"));
	return S_OK;
}

STDMETHODIMP CMainDlg::OnDiskSearch_Progress(WCHAR cDosName, DWORD dwTotalFile, DWORD dwCur)
{
	CFuncTime funcTime(_T("CMainDlg："), _T("OnDiskSearch_Progress"));
	SStatic* pPercentCtrl = FindChildByName2<SStatic>(L"percent_wnd");
	if ( pPercentCtrl )
	{
		Post([=]
		{
			static int sn = 0;
			int n = (float)dwCur/(float)dwTotalFile * 100;
			if ( n != sn)
			{
				sn = n;
				SetTabIndex(0);
				SStringT s = SStringT().Format(_T("%d%%"), n);
				pPercentCtrl->SetWindowText(s);
			}
			
		});
	}
		

	return S_OK;
}

STDMETHODIMP CMainDlg::OnDiskSearch_FileCountChange(WCHAR cDosName, DWORD dwFileCount, DWORD dwDirCount)
{
	//CFuncTime funcTime(_T("CMainDlg："), _T("OnDiskSearch_FileCountChange"));
	if ( m_pStatusCtrl )
	{
		Post([this,dwFileCount,dwDirCount]{
		
			m_pStatusCtrl->SetWindowText(SStringT().Format(_T("文件总数:%d"), dwFileCount + dwDirCount));
		});
	}
	return S_OK;
}

STDMETHODIMP CMainDlg::OnDiskSearch_StateChangeNotify(WCHAR cDosName, INT nMsg, WPARAM wParam, LPARAM lParam)
{
	CFuncTime funcTime(_T("CMainDlg："), _T("OnDiskSearch_StateChangeNotify"));
	LPCTSTR lpszCurState = _T("");
	switch( nMsg )
	{
	case DiskState_UnKnown: lpszCurState = _T("未知"); break;
	case DiskState_Ready:
		{
			Post([this]{ SetTabIndex(1);});
			break;
		}
	case DiskState_Scaning:
		{
			Post([this]{ SetTabIndex(0);});
			break;
		}
	case DiskState_Finish:
		{
			Post([this]{ SetTabIndex(1);});
			break;
		}
	case DiskState_Failed:lpszCurState = _T("失败"); break;
		break;
	}

	return S_OK;
}

STDMETHODIMP CMainDlg::OnDiskSearch_Result(DWORD dwCount, DWORD dwTickCount)
{
	CFuncTime funcTime(_T("CMainDlg："), _T("OnDiskSearch_Result"));
		m_dwCurResultIndex = -1;

		SStatic* psearch_count = FindChildByName2<SStatic>(_T("search_count"));
	 	Post([ = ]
	 	{
 	 		ListView_SetItemCount(m_pListView->m_hWnd, dwCount);
 	 		ListView_RedrawItems(m_pListView->m_hWnd,0, 20);
			if ( psearch_count )
			{
				psearch_count->SetWindowText(SStringT().Format(_T("查询结果数:%d 耗时:%0.2f秒"), dwCount, float(dwTickCount) / (float)1000));
			}
			
	 	});

	return S_OK;
}

LRESULT CMainDlg::OnListGetDispinfo(LPNMHDR lParam)
{
	LV_ITEM *pItem=&(((NMLVDISPINFO*)lParam)->item);
	if (pItem->iItem != m_dwCurResultIndex)
	{
		ZeroMemory(&m_CurResult, sizeof(m_CurResult));
		m_dwCurResultIndex = pItem->iItem;

		DWORD dwGetValueMask = 
			GetValueMask_FileSize		| //获取文件大小
			GetValueMask_MondifyTime	| //获取修改时间
			GetValueMask_CreateTime		| //获取创建时间
			GetValueMask_AccessTime		| //获取访问时间
			GetValueMask_IcoIndex		|
			GetValueMask_Attributes		; //获取属性

		if ( FAILED(m_pDiskSearch->GetResultAt(m_dwCurResultIndex, dwGetValueMask, m_CurResult)) )
		{
			return 0;
		}

	}


	if (pItem->mask & LVIF_TEXT)
	{
		switch(pItem->iSubItem)
		{
		case LC_Stat_Name:
			{
				if(pItem->mask&LVIF_IMAGE)
				{
					pItem->iImage= m_CurResult.dwIconIndex;
				}

				if (wcslen(m_CurResult.szName) == 2 && m_CurResult.szName[1] == ':'){
					DWORD dwMaxComLen = 0,dwFileSysFlag = 0;
					TCHAR szVolumeName[MAX_PATH] = {0};
					TCHAR fileSysBuf[MAX_PATH] = {0};

					CString strDriverName;
					strDriverName.Format(_T("%s\\"), m_CurResult.szName);
					UINT driveType = GetDriveType(strDriverName);
					if (!GetVolumeInformation(strDriverName,szVolumeName,MAX_PATH * 2,NULL,&dwMaxComLen,&dwFileSysFlag,fileSysBuf,MAX_PATH))
					{
						break;
					}


					CString strFormat = _tcslen(szVolumeName) ? szVolumeName : (driveType ==DRIVE_FIXED ?_T("本地磁盘") : _T("移动磁盘"));
					strFormat.AppendFormat(_T("(%s)"), m_CurResult.szName);
					_tcscpy(pItem->pszText, strFormat);
				}
				else{
					_tcscpy(pItem->pszText, m_CurResult.szName);
				}

				break;
			}
		case LC_Stat_Path:
			{
				pItem->iImage= m_CurResult.dwIconIndex;
				_tcscpy(pItem->pszText, m_CurResult.szPath);
				break;
			}

		case LC_Stat_Size:
			{

				if (m_CurResult.dwFileAttr & FILE_ATTRIBUTE_DIRECTORY)
				{
					break;
				}


				double lSize = m_CurResult.ullFileSize;
				WCHAR szInfo[MAX_PATH] = {0};
				if(m_CurResult.ullFileSize == 0) //B
				{
					swprintf(szInfo,L"%d B",(DWORD)lSize);
				}
				else if(lSize < 1024L) //B
				{
					swprintf(szInfo,L"%d B",(DWORD)lSize);
				}
				else if (lSize< 1048576) //K
				{
					swprintf(szInfo,L"%0.2f KB",lSize/1024);
				}
				else if (lSize < 1073741824) //M
				{
					swprintf(szInfo,L"%0.2f MB",lSize/1048576);
				}
				else
				{
					swprintf(szInfo,L"%0.2f GB",lSize/1073741824);
				}

				wcscpy(pItem->pszText,szInfo);

				break;
			}
		case LC_Stat_LastAccessTime:
			{
				LARGE_INTEGER llAaccessTime = { 0 };
				llAaccessTime.QuadPart = m_CurResult.ullAccessTime;

				FILETIME ftAccessTime = { 0 };
				ftAccessTime.dwLowDateTime = llAaccessTime.LowPart;
				ftAccessTime.dwHighDateTime = llAaccessTime.HighPart;



				SYSTEMTIME sysTime = {0};
				FileTimeToSystemTime(&ftAccessTime,&sysTime);
				TCHAR szTime[MAX_PATH] = {0};
				_stprintf_s(szTime, _countof(szTime), _T("%04d-%02d-%02d %02d:%02d"),
					sysTime.wYear, 
					sysTime.wMonth, 
					sysTime.wDay,
					sysTime.wHour, 
					sysTime.wMinute);

				_tcscpy(pItem->pszText, szTime);

				break;
			}
		}
	}

	return 0;
}

LRESULT CMainDlg::OnListRClick(LPNMHDR lParam)
{
	CString strSelPath;

	DWORD dwAttr = GetCurrentSelPath(strSelPath);
	RASSERT(dwAttr,0);

	CShellContextMenu scm;
	scm.SetObjects(strSelPath);

	if(INVALID_FILE_ATTRIBUTES != dwAttr || (_tcslen(strSelPath) == 2 && strSelPath.GetBuffer()[1] == ':'))
	{
		CMenu * pMenu = scm.GetMenu();
		CMenu infoMenu;
		infoMenu.LoadMenu(IDR_MENU_RCLICK);
		pMenu->AppendMenu(MF_BYCOMMAND, IDM_OPEN_CUR_PATH, _T("打开所在位置(ATL+O)"));
		pMenu->AppendMenu(MF_BYPOSITION|MF_STRING, infoMenu.GetSubMenu(0), _T("文件信息"));
		if (dwAttr & FILE_ATTRIBUTE_DIRECTORY)
		{
			pMenu->AppendMenu(MF_BYCOMMAND, IDM_SEARCH_CUR_PATH, _T("搜索当前目录"));
		}
	}

	POINT	curPos;
	GetCursorPos(&curPos);
	try
	{
		scm.ShowContextMenu (m_pListView->m_hWnd, curPos);
	}
	catch(...){}

	return 0;
}

LRESULT CMainDlg::OnListClick(LPNMHDR lParam)
{
	

	return 0;
}

LRESULT CMainDlg::OnListMenuOpenCurPath(LPNMHDR lParam)
{
	RASSERT(m_pListView && m_pListView->m_hWnd == lParam->hwndFrom, 0);


	CString strLastSelectPath ;
	GetCurrentSelPath(strLastSelectPath);

	std::wstring sFolder = _T( "/Select,\"" ) + strLastSelectPath + _T("\"");

	// Prepare shell execution params
	SHELLEXECUTEINFO shExecInfo   = { 0 };
	shExecInfo.cbSize             = sizeof(shExecInfo);
	shExecInfo.lpFile             = _T("explorer.exe");
	shExecInfo.lpParameters       = sFolder.c_str();
	shExecInfo.nShow              = SW_SHOWNORMAL;
	shExecInfo.lpVerb             = _T("open"); // Context menu item
	shExecInfo.fMask              = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;

	try
	{
		ShellExecuteEx(&shExecInfo);
	}
	catch(...){}

	return 0;
}

LRESULT CMainDlg::OnListMenuCopyName(LPNMHDR lParam)
{
	RASSERT(m_pListView && m_pListView->m_hWnd == lParam->hwndFrom, 0);


	CString strLastSelectPath ;
	GetCurrentSelPath(strLastSelectPath);
	msapi::WriteAsciiStringToClipboard(PathFindFileName(strLastSelectPath),NULL);
	return 0;
}

LRESULT CMainDlg::OnListMenuCopyPath(LPNMHDR lParam)
{
	RASSERT(m_pListView && m_pListView->m_hWnd == lParam->hwndFrom, 0);

	CString strLastSelectPath ;
	GetCurrentSelPath(strLastSelectPath);

	TCHAR szPath[MAX_PATH] = {0};
	_tcscpy_s(szPath,MAX_PATH, strLastSelectPath);

	TCHAR* pTail = (TCHAR*)_tcsrchr(szPath, _T('\\'));
	if (NULL != pTail){
		*pTail = 0;
	}

	msapi::WriteAsciiStringToClipboard(szPath,NULL);

	return 0;
}

LRESULT CMainDlg::OnListMenuCopyFullPath(LPNMHDR lParam)
{
	RASSERT(m_pListView && m_pListView->m_hWnd == lParam->hwndFrom, 0);
	CString strLastSelectPath ;
	GetCurrentSelPath(strLastSelectPath);
	msapi::WriteAsciiStringToClipboard(strLastSelectPath,NULL);
	return 0;
}

LRESULT CMainDlg::OnListDropeffectCopy(LPNMHDR lParam)
{
	RASSERT(m_pListView && m_pListView->m_hWnd == lParam->hwndFrom, 0);
	CString strLastSelectPath ;
	GetCurrentSelPath(strLastSelectPath);
	USES_CONVERSION;
	msapi::CopyFileToClipboard(W2A(strLastSelectPath.GetBuffer()), DROPEFFECT_COPY);
	return 0;
}

LRESULT CMainDlg::OnListDropeffectMove(LPNMHDR lParam)
{
	RASSERT(m_pListView && m_pListView->m_hWnd == lParam->hwndFrom, 0);

	CString strLastSelectPath ;
	GetCurrentSelPath(strLastSelectPath);
	USES_CONVERSION;
	msapi::CopyFileToClipboard(W2A(strLastSelectPath.GetBuffer()), DROPEFFECT_MOVE);
	return 0;
}

LRESULT CMainDlg::OnListCustomDraw(LPNMHDR lParam)
{
	RASSERT(m_pListView && m_pListView->m_hWnd == lParam->hwndFrom, 0);
	LPNMLVCUSTOMDRAW pNMCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);
	if ( CDDS_PREPAINT == pNMCD->nmcd.dwDrawStage )
	{
		//return SetWindowLongPtr(m_pResultList->m_hWnd, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
		return CDRF_NOTIFYITEMDRAW;
	}

	if(pNMCD->nmcd.dwDrawStage  == (CDDS_ITEMPREPAINT )) 	
	{
		if(m_pListView->GetItemState( pNMCD->nmcd.dwItemSpec,LVIS_SELECTED) == LVIS_SELECTED)
		{
			pNMCD->clrTextBk = RGB(51,153,255);
			return CDRF_NEWFONT; 
		}
		if(pNMCD->nmcd.dwItemSpec % 2 != 0)
		{
			pNMCD->clrTextBk  = RGB(249,249,249);  
		}
		return CDRF_NEWFONT; 
	} 

	return CDRF_DODEFAULT;
}

LRESULT CMainDlg::OnListDBClick(LPNMHDR lParam)
{
	RASSERT(m_pListView && m_pListView->m_hWnd == lParam->hwndFrom, 0);
	CString strPath;
	RASSERT(GetCurrentSelPath(strPath), 0);
	RASSERT(PathFileExists(strPath),0);

	//如果是EXE
	CString strExt = PathFindExtension(strPath);
	if ( strExt.CompareNoCase(_T(".exe")) == 0)
	{
		SHELLEXECUTEINFO shellInfo = { 0 };
		shellInfo.cbSize = sizeof( shellInfo );
		shellInfo.lpVerb = _T("open");
		shellInfo.hwnd = NULL;
		shellInfo.lpFile =(LPCWSTR)strPath;
		shellInfo.nShow = SW_SHOWNORMAL;
		shellInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
		ShellExecuteEx( &shellInfo );

	}
	else
	{
		NMHDR nmh;
		nmh.code = IDM_OPEN_CUR_PATH;    // Message type defined by control.
		nmh.idFrom = GetDlgCtrlID(m_pListView->m_hWnd);
		nmh.hwndFrom = m_pListView->m_hWnd;

		return ::SendMessage(::GetParent(m_pListView->m_hWnd), WM_NOTIFY, (WPARAM)m_pListView->m_hWnd, (LPARAM)&nmh);
	}

	return 0;
}

LRESULT CMainDlg::OnListKeyDown(LPNMHDR lParam)
{
	LV_KEYDOWN* pLVKeyDow = (LV_KEYDOWN*)lParam;

	if ( pLVKeyDow->wVKey == 'C' && HIBYTE(GetKeyState(VK_SHIFT))  && HIBYTE(GetKeyState(VK_CONTROL))  )
	{
		NMHDR nmh;
		nmh.code = IDM_COPY_PATH;    // Message type defined by control.
		nmh.idFrom = GetDlgCtrlID(m_pListView->m_hWnd);
		nmh.hwndFrom = m_pListView->m_hWnd;

		return ::SendMessage(::GetParent(m_pListView->m_hWnd), WM_NOTIFY, (WPARAM)m_pListView->m_hWnd, (LPARAM)&nmh);
	}

	//复制文件
	if ( pLVKeyDow->wVKey == 'C' && HIBYTE(GetKeyState(VK_CONTROL)))
	{
		NMHDR nmh;
		nmh.code = MN_DROPEFFECT_COPY_MSG;    // Message type defined by control.
		nmh.idFrom = GetDlgCtrlID(m_pListView->m_hWnd);
		nmh.hwndFrom = m_pListView->m_hWnd;

		return ::SendMessage(::GetParent(m_pListView->m_hWnd), WM_NOTIFY, (WPARAM)m_pListView->m_hWnd, (LPARAM)&nmh);
	}
	
	//复制文件名
	if ( pLVKeyDow->wVKey == 'C' && HIBYTE(GetKeyState(VK_SHIFT) ))
	{
		NMHDR nmh;
		nmh.code = IDM_COPY_NAME;    // Message type defined by control.
		nmh.idFrom = GetDlgCtrlID(m_pListView->m_hWnd);
		nmh.hwndFrom = m_pListView->m_hWnd;

		return ::SendMessage(::GetParent(m_pListView->m_hWnd), WM_NOTIFY, (WPARAM)m_pListView->m_hWnd, (LPARAM)&nmh);
	}

	//复制全路径
	if ( pLVKeyDow->wVKey == 'C' && HIBYTE(GetKeyState(VK_MENU) ) )
	{
		NMHDR nmh;
		nmh.code = IDM_COPY_FULL_PATH;    // Message type defined by control.
		nmh.idFrom = GetDlgCtrlID(m_pListView->m_hWnd);
		nmh.hwndFrom = m_pListView->m_hWnd;

		return ::SendMessage(::GetParent(m_pListView->m_hWnd), WM_NOTIFY, (WPARAM)m_pListView->m_hWnd, (LPARAM)&nmh);
	}

	//复制路径
	

	if ( pLVKeyDow->wVKey == 'O' && GetKeyState(VK_MENU) < 0 )
	{
		NMHDR nmh;
		nmh.code = IDM_OPEN_CUR_PATH;    // Message type defined by control.
		nmh.idFrom = GetDlgCtrlID(m_pListView->m_hWnd);
		nmh.hwndFrom = m_pListView->m_hWnd;

		return ::SendMessage(::GetParent(m_pListView->m_hWnd), WM_NOTIFY, (WPARAM)m_pListView->m_hWnd, (LPARAM)&nmh);
	}
	return 0;
}

LRESULT CMainDlg::OnListItemChanged(LPNMHDR lParam)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)lParam;

	//SendMessage(WM_NOTIFY, IDC_RESULT_LIST,(LPARAM)&lParam);
	//SetMsgHandled(TRUE);

	
	if( pNMListView->uChanged==LVIF_STATE && pNMListView->uNewState & LVIS_SELECTED )
	{
		int nItem = pNMListView->iItem;
		CString str;str.Format(_T("%d\r\n"), nItem);
		OutputDebugString(str);
		Post([this,nItem]{SetSelectInformation(nItem);});
	}


	//SetMsgHandled(FALSE);

	return 0;
}

LRESULT CMainDlg::OnExecute(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL/* bHandled*/)
{
	auto func = (std::function<void()> *)lParam;
	(*func)();
	delete func;
	return 0;
}

VOID CMainDlg::OnSearch(EventArgs *pEvt)
{
	BOOL bSearch = FALSE;
	if ( !pEvt )
		bSearch = TRUE;

	else if ( pEvt->GetID() == EVT_RE_NOTIFY)
	{
		EventRENotify* pRENotify = static_cast<EventRENotify*>(pEvt);
		if ( pRENotify->iNotify == EN_CHANGE)
		{
			bSearch = TRUE;
		} 
	}
	
	if ( !bSearch )
	{
		return ;
	}


	m_dwQueryMask  = (QueryConditionMask_Dir | QueryConditionMask_File | QueryConditionMask_Sub);
	m_strExten.Empty();
	m_strQueryText.Empty();
	m_strQueryPath.Empty();
	
	GetQueryPath();
	GetQueryText();
	GetQueryExtension();
	GetQueryConditionMask();

	QueryCondition Query;
	Query.dwSize = sizeof( Query );
	Query.dwConditionMask	= m_dwQueryMask;
	Query.lpstrFileName		= m_strQueryText;
	Query.lpstrExtension	= m_strExten ;
	Query.lpstrIncludeDir	= m_strQueryPath;

	if( m_pDiskSearch )
		m_pDiskSearch->Query( Query );
}

VOID CMainDlg::OnCheckUpdata(EventArgs *pEvt /*= NULL*/)
{
	//启动更新程序
	CString strModulePath;
	msapi::GetCurrentPath(strModulePath.GetBufferSetLength(MAX_PATH), MAX_PATH);
	strModulePath.ReleaseBuffer();
	strModulePath.Append(_T("\\update.exe"));

	SHELLEXECUTEINFO shellInfo = { 0 };
	shellInfo.cbSize = sizeof( shellInfo );
	shellInfo.lpVerb = _T("open");
	shellInfo.hwnd = NULL;
	shellInfo.lpFile =(LPCWSTR)strModulePath;
	shellInfo.nShow = SW_SHOWNORMAL;
	shellInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
	ShellExecuteEx( &shellInfo );
}

VOID CMainDlg::OnMark_0(EventArgs *pEvt /*= NULL*/)
{
	SetCurFileMark( FileLeave_0);
}

VOID CMainDlg::OnMark_1(EventArgs *pEvt /*= NULL*/)
{
	SetCurFileMark( FileLeave_1);
}

VOID CMainDlg::OnMark_2(EventArgs *pEvt /*= NULL*/)
{
	SetCurFileMark( FileLeave_2);
}

VOID CMainDlg::OnMark_3(EventArgs *pEvt /*= NULL*/)
{
	SetCurFileMark( FileLeave_3);
}

VOID CMainDlg::OnMark_4(EventArgs *pEvt /*= NULL*/)
{
	SetCurFileMark( FileLeave_4);
}

VOID CMainDlg::OnMark_5(EventArgs *pEvt /*= NULL*/)
{
	SetCurFileMark( FileLeave_5);
}

VOID CMainDlg::OnMark_6(EventArgs *pEvt /*= NULL*/)
{
	SetCurFileMark( FileLeave_6);
}

VOID CMainDlg::OnMark_7(EventArgs *pEvt /*= NULL*/)
{
	SetCurFileMark( FileLeave_7);
}

BOOL CMainDlg::GetQueryExtension()
{
	

	SEdit* psearch_input = FindChildByName2<SEdit>(_T("search_input"));
	if( psearch_input ) m_strExten = psearch_input->GetWindowText();


	INT nPos = m_strExten.Find('.');
	if (nPos != -1)
	{
		m_strExten = m_strExten.Mid(nPos);
	}
	else
	{
		m_strExten.Empty();
	}

	m_strExten.Trim();
	m_strExten.Trim('.');

	if ( !m_strExten.IsEmpty())
	{
		return TRUE;
	}
	
	SRadioBox* opt_type_qb= FindChildByName2<SRadioBox>(_T("opt_type_qb"));
	SRadioBox* opt_type_tb= FindChildByName2<SRadioBox>(_T("opt_type_tb"));
	SRadioBox* opt_type_wd= FindChildByName2<SRadioBox>(_T("opt_type_wd"));
	SRadioBox* opt_type_sp= FindChildByName2<SRadioBox>(_T("opt_type_sp"));
	SRadioBox* opt_type_yp= FindChildByName2<SRadioBox>(_T("opt_type_yp"));
	SRadioBox* opt_type_ys= FindChildByName2<SRadioBox>(_T("opt_type_ys"));
	SRadioBox* opt_type_cx= FindChildByName2<SRadioBox>(_T("opt_type_cx"));
	SRadioBox* opt_type_ml= FindChildByName2<SRadioBox>(_T("opt_type_ml"));

	if ( opt_type_qb && opt_type_qb->IsChecked() )
	{
		m_dwQueryMask |= (QueryConditionMask_Dir | QueryConditionMask_File);
		m_strExten.Empty();
		return TRUE;
	}

	if ( opt_type_tb && opt_type_tb->IsChecked() )
	{
		m_strExten =  _T("ani;bmp;gif;ico;jpe;jpeg;jpg;pcx;png;psd;tga;tif;tiff;wmf");
	}

	if ( opt_type_wd && opt_type_wd->IsChecked() )
	{
		m_strExten = _T("c;chm;cpp;cxx;doc;docm;docx;dot;dotm;dotx;h;hpp;htm;html;hxx;ini;java;js;lua;mht;mhtml;pdf;potx;potm;ppam;ppsm;ppsx;pps;ppt;pptm;pptx;rtf;sldm;sldx;thmx;txt;vsd;wpd;wps;wri;xlam;xls;xlsb;xlsm;xlsx;xltm;xltx;xml");
	}

	if ( opt_type_sp && opt_type_sp->IsChecked() )
	{
		m_strExten =  _T("3g2;3gp;3gp2;3gpp;amr;asf;avi;bik;d2v;divx;drc;dsa;dsm;dss;dsv;flc;fli;flic;flv;ifo;ivf;m1v;m2v;m4b;m4p;m4v;mkv;mp2v;mp4;mpe;mpeg;mpg;mpv2;mov;ogm;pss;pva;qt;ram;ratdvd;rm;rmm;rmvb;roq;rpm;smk;swf;tp;tpr;ts;vob;vp6;wm;wmp;wmv");
	}

	if ( opt_type_yp && opt_type_yp->IsChecked() )
	{
		m_strExten = _T("aac;ac3;aif;aifc;aiff;au;cda;dts;fla;flac;it;m1a;m2a;m3u;m4a;mid;midi;mka;mod;mp2;mp3;mpa;ogg;ra;rmi;spc;rmi;snd;umx;voc;wav;wma;xm");
	}

	if ( opt_type_ys && opt_type_ys->IsChecked() )
	{
		m_strExten =  _T("7z;ace;arj;bz2;cab;gz;gzip;r00;r01;r02;r03;r04;r05;r06;r07;r08;r09;r10;r11;r12;r13;r14;r15;r16;r17;r18;r19;r20;r21;r22;r23;r24;r25;r26;r27;r28;r29;rar;tar;tgz;z;zip");
	}

	if ( opt_type_cx && opt_type_cx->IsChecked() )
	{
		m_strExten =  _T("ext:bat;cmd;exe;msi;scr");
	}

	if ( opt_type_ml && opt_type_ml->IsChecked() )
	{
		m_dwQueryMask |= QueryConditionMask_Dir;
		m_dwQueryMask &= ~QueryConditionMask_File;
		m_strExten.Empty();
	}

	if ( m_strExten.GetLength() )
	{
		m_dwQueryMask |= QueryConditionMask_FullExt;
	}

	
	return TRUE;
}

BOOL CMainDlg::GetQueryConditionMask()
{
	SCheckBox* pchk_word_mpp = FindChildByName2<SCheckBox>(_T("chk_word_mpp"));
	SCheckBox* pchk_case_mpp = FindChildByName2<SCheckBox>(_T("chk_case_mpp"));
	SCheckBox* pchk_sys_mpp = FindChildByName2<SCheckBox>(_T("chk_sys_mpp"));
	SCheckBox* pchk_hidden_mpp = FindChildByName2<SCheckBox>(_T("chk_hidden_mpp"));
	SCheckBox* pinclue_child_path = FindChildByName2<SCheckBox>(_T("inclue_child_path"));

	//DWORD dwQueryConditionMask = QueryConditionMask_File | QueryConditionMask_Dir;

	if ( pchk_word_mpp && pchk_word_mpp->IsChecked() )
		m_dwQueryMask |= QueryConditionMask_Full;

	if ( pchk_case_mpp && pchk_case_mpp->IsChecked() )
		m_dwQueryMask |=  QueryConditionMask_Case;

	if ( pchk_case_mpp && pchk_sys_mpp->IsChecked() )
		m_dwQueryMask |= QueryConditionMask_Exclude_System;

	if( pchk_hidden_mpp && pchk_hidden_mpp->IsChecked() )
		m_dwQueryMask |= QueryConditionMask_Exclude_Hidden;

	if( pinclue_child_path && !pinclue_child_path->IsChecked() )
		m_dwQueryMask &= ~QueryConditionMask_Sub;
	

	if ( m_strExten.GetLength() )
	{
		m_dwQueryMask &= ~QueryConditionMask_Dir;
	}

	return m_dwQueryMask;
}

BOOL CMainDlg::GetQueryText()
{
	CString strText;
	SEdit* psearch_input = FindChildByName2<SEdit>(_T("search_input"));
	if ( psearch_input )
		m_strQueryText =  psearch_input->GetWindowText();
	
	
	
	INT nPos = m_strQueryText.Find('.');
	if (nPos != -1)
	{
		m_strQueryText = m_strQueryText.Left(nPos);
	}

	m_strQueryText.Trim();
	if (m_strQueryText.GetLength())
	{
		m_strQueryText = m_strQueryText;
	}

	return TRUE;
}

BOOL CMainDlg::GetQueryPath()
{
	SEdit* ppath_input = FindChildByName2<SEdit>(_T("path_input"));
	if ( ppath_input )
		m_strQueryPath =  (LPCWSTR)ppath_input->GetWindowText();
	
	return TRUE;
}


VOID CMainDlg::SetItemDescription(LPCWSTR lpszFileName)
{

}

BOOL CMainDlg::SetCurFileMark(int nMark)
{
	SearchResult searchResult = { 0 };
	if ( !GetCurrentSel(searchResult) )
		return FALSE;

	if ( searchResult.dwLeave == nMark )
		return TRUE;

	CString strPath;
	strPath.Format(_T("%s%s"), searchResult.szPath,searchResult.szName);
	return SUCCEEDED(m_pDiskSearch->SetFileMark( strPath, nMark));
}

VOID CMainDlg::OnWorkCheckUpdata()
{
	network::CHttpImplement http;
	network::CUrlParamValueMap urlParamMap;
	TCHAR szDiskSer[MAX_PATH] = { 0 };
	TCHAR szVer[ MAX_PATH ] = { 0 };
	TCHAR szOper[MAX_PATH] = { 0 };

	msapi::CApp softApp(GetEnvParamString("productname"));

	urlParamMap["prouctid"] = softApp.GetSetting(_T("prouctid"), 0);
	urlParamMap["packetid"] = softApp.GetSetting(_T("packetid"), 0);
	urlParamMap["version"]	= softApp.GetVersion(szVer, MAX_PATH);
	urlParamMap["oper"]		= softApp.GetSetting(_T("oper"), _T("0"), szOper, MAX_PATH);
	urlParamMap["hdd"]	    = msapi::CApp::GetDiskSerial(szDiskSer, MAX_PATH);

	if ( http.GetRequest("api.kyan.com.cn", 80, "jcdisk/update", urlParamMap) )
	{
		
		return ;
	}

	Json::Value root;
	if ( !Json::Reader().parse((const char*)http.GetRequestData(), root) )
	{
		return ;
	}

	Json::Value retCode =  root["code"];
	if (retCode.type() != Json::intValue )
	{
		return ;
	}

	//已是最新版本
	if ( retCode.asInt() == 0 )
	{
		return ;
	}

	//检测到最新版本
	if (  retCode.asInt() == 1 )
	{
		Json::Value Msg = root["msg"];
		CString strNetVer = Msg["version"].asWString().c_str();

		TCHAR szVersion[ MAX_PATH ] = { 0 };
		msapi::CApp(GetEnvParamString("productname")).GetVersion(szVersion,MAX_PATH);
		if ( CVerCmp::CompareStr(strNetVer, szVersion ) > 0)
		{
			SWindow* pPanel_1 = FindChildByName2<SWindow>(_T("check_canp_updata"));
			SWindow* pPanel = FindChildByName2<SWindow>(_T("check_updata"));
			Post([pPanel, pPanel_1]
			{
				if( pPanel_1 ) pPanel_1->SetVisible(TRUE,TRUE);
				if( pPanel ) pPanel->SetVisible(FALSE,TRUE);
			});

			GrpMsg(GroupName, MsgLevel_Msg, _T("发现新版本:%s -> %s"), szVersion, strNetVer);
			
			
		}
	}

}

VOID CMainDlg::SetSelectInformation(int nItem)
{
	CString strSelPath;

	DWORD dwAttr = 0;
	SearchResult CurResult = { 0 };
	if ( !GetSelect(CurResult, nItem))
	{
		return ;
	}

	CString str;str.Format(_T("%d\r\n"), nItem);
	OutputDebugString(str);

	strSelPath.Format(_T("%s%s"), CurResult.szPath,CurResult.szName);
	dwAttr = CurResult.dwFileAttr;
	RASSERT(dwAttr,);

	
	SIconWnd* sIco = FindChildByName2<SIconWnd>(_T("show_ico_wnd"));
	if ( sIco && sIco->IsVisible())
		sIco->SetVisible(FALSE,TRUE);

	SImageWnd* sImage = FindChildByName2<SImageWnd>(_T("show_image_erweima"));

	if( sImage && sImage->IsVisible())
		sImage->SetVisible(FALSE,TRUE);


	SFreeImageWnd* sFreeImage = FindChildByName2<SFreeImageWnd>(_T("show_freeimage_wnd"));
	if( sFreeImage && sFreeImage->IsVisible())
		sFreeImage->SetVisible(FALSE,TRUE);

	SShowImageWnd* sImageWnd = FindChildByName2<SShowImageWnd>(_T("show_image_wnd"));
	if( sImageWnd && sImageWnd->IsVisible())
		sImageWnd->SetVisible(FALSE,TRUE);

	SShowGifWnd * sGifWnd = FindChildByName2<SShowGifWnd>(_T("show_gif_wnd"));
	if( sGifWnd && sGifWnd->IsVisible())
		sGifWnd->SetVisible(FALSE,TRUE);

	BOOL bSuccess = FALSE;
	LPCTSTR lpszFileExtension = PathFindExtension(strSelPath);
	if (	_tcsicmp(lpszFileExtension, _T(".png")) == 0 || 
		_tcsicmp(lpszFileExtension, _T(".jpg")) == 0 || 
		_tcsicmp(lpszFileExtension, _T(".bmp")) == 0 || 
		_tcsicmp(lpszFileExtension, _T(".pcx")) == 0 || 
		_tcsicmp(lpszFileExtension, _T(".ico")) == 0 ||
		_tcsicmp(lpszFileExtension, _T(".jpeg")) == 0 
		)
	{
		bSuccess = sImageWnd->SetImageFile(strSelPath);
		if ( bSuccess )
			sImageWnd->SetVisible(TRUE,TRUE);
	}
	else if ( _tcsicmp(lpszFileExtension, _T(".gif")) == 0 )
	{
		bSuccess = sGifWnd->SetImageFile(strSelPath);
		if( bSuccess )
			sGifWnd->SetVisible(TRUE,TRUE);
		
	}
	else if ( _tcsicmp(lpszFileExtension, _T(".psd")) == 0 )
	{
		bSuccess = sFreeImage->SetImage(strSelPath);
		if ( bSuccess )
			sFreeImage->SetVisible(TRUE,TRUE);
		
	}
	else
	{
		SHFILEINFOW sfi = { 0 };
		SHGetFileInfoW(strSelPath,FILE_ATTRIBUTE_NORMAL,&sfi,sizeof(sfi),SHGFI_ICON|SHGFI_LARGEICON ) ;
		if ( sfi.hIcon )
		{
			sIco->SetIcon(sfi.hIcon);
			sIco->SetVisible(TRUE,TRUE);
			bSuccess = TRUE;
		}
	}

	if ( !bSuccess )
	{
		sImage->SetVisible(TRUE,TRUE);
	}

	

	//设置标记
	SRadioBox* R = FindChildByName2<SRadioBox>(g_CtrlMap[CurResult.dwLeave]);
	if ( R )
	{
		R->SetCheck(TRUE);
	}
	
}

DWORD CMainDlg::GetCurrentSelPath(CString& strPath)
{
	SearchResult CurResult = { 0 };
	if ( GetCurrentSel(CurResult))
	{
		strPath.Format(_T("%s%s"), CurResult.szPath,CurResult.szName);
		return CurResult.dwFileAttr;
	}

	return 0;
}

BOOL CMainDlg::GetCurrentSel(SearchResult& Result)
{
	RASSERT(m_pListView, FALSE);
	int nSelIndex = m_pListView->GetSelectedIndex();
	return GetSelect(Result, nSelIndex);
}

BOOL CMainDlg::GetSelect(SearchResult& Result , int nSelIndex)
{
	RASSERT(m_pListView, FALSE);
	RASSERT(nSelIndex != -1, FALSE);
	RASSERT(m_pDiskSearch, 0);



	DWORD dwGetValueMask = GetValueMask_Attributes;

	if ( FAILED(m_pDiskSearch->GetResultAt(nSelIndex, dwGetValueMask, Result)) )
	{
		return 0;
	}

	return TRUE;
}
