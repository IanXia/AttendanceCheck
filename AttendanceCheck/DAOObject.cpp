#include "stdafx.h"
#include "DAOObject.h"
#include <iostream>


DAOObject::DAOObject(const _TCHAR* file) :m_recSet(&m_db)
{
	m_file = file;
}


DAOObject::~DAOObject()
{
	if (m_db.IsOpen())
		m_db.Close();
}

bool DAOObject::open()
{
	bool ret = false;
	try
	{
		if (!m_db.IsOpen())
		{
			m_db.Open(m_file);
			ret = m_db.IsOpen();
		}
		ret = true;
	}
	catch(CException* e)
	{
		TCHAR text[512];
		e->GetErrorMessage(text, 512);
		cout << "Open file error , Exception:";
		wcout<< text << endl;
		e->Delete();
	}
	return ret;
}

bool DAOObject::select(const CString & sql, DAOObject::selRet& ret)
{
	if(!open())
		return false;
	ret.clear();
	m_recSet.Open(AFX_DAO_USE_DEFAULT_TYPE, sql , NULL);
	while (!m_recSet.IsEOF())	// 有没有到表结尾
	{
		int count = m_recSet.GetFieldCount();
		vector<row> row(count);
		for (int i = 0; i < count; ++i)
		{
			m_recSet.GetFieldInfo(i, row[i].info);
			m_recSet.GetFieldValue(i, row[i].var);
		}
		ret.push_back(std::move(row));
		m_recSet.MoveNext();
	}
	m_recSet.Close();
	return true;
}
bool DAOObject::excute(const CString & sql)
{
	if (!open())
		return false;
	m_db.Execute(sql);
	return true;
}

bool DAOObject::selectEach(const CString & sql, std::function<void(CDaoRecordset*)> each)
{
	if (!open())
		return false;
	m_recSet.Open(AFX_DAO_USE_DEFAULT_TYPE, sql, NULL);
	while (!m_recSet.IsEOF())	// 有没有到表结尾
	{
		each(&m_recSet);
		m_recSet.MoveNext();
	}
	m_recSet.Close();
	return true;
}