#pragma once
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxdao.h>
#endif // _AFX_NO_AFXCMN_SUPPORT
#include <vector>
#include <functional>
using namespace std;
struct row
{
	CDaoFieldInfo info;
	COleVariant var;
};
class DAOObject
{
public:
	typedef vector< vector<row>> selRet;
	DAOObject(const _TCHAR* file);
	~DAOObject();
	bool select(const CString & sql, DAOObject::selRet& ret);
	bool selectEach(const CString & sql, std::function<void(CDaoRecordset*)> each);
	bool excute(const CString & sql);
private:
	bool open();
	CDaoDatabase m_db;				//数据库
	CDaoRecordset m_recSet;		//记录集
	CString	m_file;
};

