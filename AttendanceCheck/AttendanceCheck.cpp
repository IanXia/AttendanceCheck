// AttendanceCheck.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "DAOObject.h"
#include <iostream>
#include <functional>
#include <unordered_map>
#include <unordered_set>
using namespace std;

extern pair<time_t, time_t> sWorkTime[2][2];

string buildStr(WCHAR * str)
{
	WCHAR ch = str[0];
	string ret;
	while (ch != 0)
	{
		ret.push_back((char)ch);
		ch = *++str;
	}
	ret.push_back(0);
	return std::move(ret);
}
inline time_t mktime(time_t hour, time_t min, time_t sec)
{
	return hour * 3600 + min * 60 + sec;
}
struct CheckHelper
{
	std::function<void(DAOObject *, CDaoRecordset&)> eachRow;
	std::function<void(DAOObject *)> finish;
};

class shortTime
{
	enum Type{
		NONE,
		YEAR,
		MONTH,
		DAY,
		HOUR,
		MINUTS,
		SECOND,
		AMPM
	};
	struct Location{
		int pos = -1;
		int num = 0;
		_TCHAR separator = 0;
		bool isUpper = false;
	};
	struct TimeItem{
		Type type;
		Location info;
		TimeItem()
		{
			type = NONE;
		}
		TimeItem(Type t)
		{
			type = t;
		}
	};

public:
	shortTime() :m_order(10)
	{
		ATL::CRegKey reg;
		if (reg.Open(HKEY_CURRENT_USER, _T("Control Panel\\International"), KEY_READ) == ERROR_SUCCESS)
		{
			_TCHAR buf[256] = {};
			ULONG nChars = 256;
			if (reg.QueryStringValue(_T("sTimeFormat"), buf, &nChars) == ERROR_SUCCESS)
			{
				sTimeFormat = buf;
			}
			if (reg.QueryStringValue(_T("sShortDate"), buf, &nChars) == ERROR_SUCCESS)
			{
				sShortDate = buf;
			}
			if (reg.QueryStringValue(_T("s1159"), buf, &nChars) == ERROR_SUCCESS)
			{
				s1159 = buf;
			}
			if (reg.QueryStringValue(_T("s2359"), buf, &nChars) == ERROR_SUCCESS)
			{
				s2359 = buf;
			}
			_TCHAR dateSeparator = 0;
			auto fun = [](const _TCHAR * buf, int size, _TCHAR ch){
				Location loc = {};
				int pos = 0;
				for (int i = 0; i < size; ++i)
				{
					if (!isalpha(buf[i]))
					{
						if (loc.num > 0)
						{
							loc.separator = buf[i];
							break;
						}
						++pos;
					}
					if (buf[i] == ch || (loc.isUpper = (toupper(ch) == buf[i])))
					{
						if (loc.num == 0)
						{
							loc.pos = pos;
						}
						++loc.num;
					}
				}
				return loc;
			};
			TimeItem tmp = { YEAR };
			tmp.info = fun(sShortDate.GetBuffer(), sShortDate.GetLength(), _T('y'));
			if (tmp.info.pos != -1)
			{
				m_order[tmp.info.pos % 3] = tmp;
			}
			tmp = { MONTH };
			tmp.info = fun(sShortDate.GetBuffer(), sShortDate.GetLength(), _T('M'));
			if (tmp.info.pos != -1)
			{
				m_order[tmp.info.pos % 3] = tmp;
			}
			tmp = { DAY };
			tmp.info = fun(sShortDate.GetBuffer(), sShortDate.GetLength(), _T('d'));
			if (tmp.info.pos != -1)
			{
				m_order[tmp.info.pos % 3] = tmp;
			}
			tmp = { HOUR };
			tmp.info = fun(sTimeFormat.GetBuffer(), sTimeFormat.GetLength(), _T('h'));
			if (tmp.info.pos != -1)
			{
				m_order[tmp.info.pos + 3] = tmp;
			}
			tmp = { MINUTS };
			tmp.info = fun(sTimeFormat.GetBuffer(), sTimeFormat.GetLength(), _T('m'));
			if (tmp.info.pos != -1)
			{
				m_order[tmp.info.pos + 3] = tmp;
			}
			tmp = { SECOND };
			tmp.info = fun(sTimeFormat.GetBuffer(), sTimeFormat.GetLength(), _T('s'));
			if (tmp.info.pos != -1)
			{
				m_order[tmp.info.pos + 3] = tmp;
			}
			tmp = { AMPM };
			tmp.info = fun(sTimeFormat.GetBuffer(), sTimeFormat.GetLength(), _T('t'));
			if (tmp.info.pos != -1)
			{
				m_order[tmp.info.pos + 3] = tmp;
			}
		}
	}
	CString mkShortTime(SYSTEMTIME tm)
	{
		int lastPos = 0;
		CString format, cTmp, cBuild;
		int data = 0;
		for (size_t i = 0; i < m_order.size(); i++)
		{
			TimeItem tmp = m_order[i];
			if (tmp.type == NONE)
			{
				continue;
			}
			if (lastPos > tmp.info.pos)
			{
				format.AppendChar(_T(' '));
			}
			cTmp.Empty();
			switch (tmp.type)
			{
			case YEAR:
				if (tmp.info.num == 2)
				{
					data = tm.wYear % 100;
				}
				goto R;
			case HOUR:
				data = tm.wHour;
				goto R;
			case MONTH:
				data = tm.wMonth;
				goto R;
			case DAY:
				data = tm.wDay;
				goto R;
			case MINUTS:
				data = tm.wMinute;
				goto R;
			case SECOND:
				data = tm.wSecond;
			R:				cTmp.AppendChar(_T('%'));
				if (tmp.info.num == 2)
				{
					cTmp.AppendChar(_T('0'));
				}
				if (tmp.info.num > 1)
				{
					cTmp.AppendChar(tmp.info.num + _T('0'));
				}
				cTmp.AppendChar(_T('d'));
				cBuild.Format(cTmp, data);
				format += cBuild;
				break;
			case AMPM:
				format.Append(_T("XX"));
				break;
			};
			if (tmp.info.separator != 0)
			{
				format.AppendChar(tmp.info.separator);
			}
			lastPos = tmp.info.pos;
		}
		return format;
	}
private:
	CString sTimeFormat;
	CString sShortDate;
	CString s1159;
	CString s2359;
	vector<TimeItem> m_order;
};
class CheckAttendance
{
public:
	CheckAttendance(DAOObject* dao, _TCHAR* name) :m_dao(dao), m_name(name)
	{
		row id;
		if (!getIDFromName(name, id))
		{
			wprintf(L"找不到用户:%s\n", name);
			cout << "爷不干了。。。Bye" << endl;
			throw 1;
		}
		if (id.var.vt == 0)
		{
			cout << "找不到用户ID，爷不干了。。。Bye" << endl;
			throw 1;
		}
		m_id = id.var.intVal;
	}
	bool getIDFromName(_TCHAR* name, row& id)
	{
		DAOObject::selRet ret;
		bool isFind = false;
		if (m_dao->select(_T("SELECT * FROM USERINFO"), ret))
		{
			for (size_t i = 0; i < ret.size(); ++i)
			{
				row tmp = {};
				for (size_t j = 0; j < ret[i].size(); j++)
				{
					if (ret[i][j].info.m_strName == L"USERID")
					{
						tmp = ret[i][j];
					}
					if (ret[i][j].info.m_strName == L"Name")
					{
						BSTR bstr = ret[i][j].var.bstrVal;
						//string s = buildStr(bstr);
						CString cs(bstr);
						isFind = cs == name;
					}
					if (isFind && tmp.info.m_strName == L"USERID")
					{
						id = tmp;
						return true;
					}
				}
			}
		}
		return false;
	}

	bool checkDate(const CString& sql, CheckHelper* helper)
	{
		auto each = [this, helper](CDaoRecordset * set){
			if (helper->eachRow)
			{
				helper->eachRow(m_dao, *set);
			}
		};
		bool ret = m_dao->selectEach(sql, each);
		if (helper->finish)
		{
			helper->finish(m_dao);
		}
		return ret;
	}

	int getID(){ return m_id; }
private:
	DAOObject * m_dao;
	int m_id;
	CString m_name;
};
enum workTimeType
{
	AM,
	PM,
	NONE
};
enum checkType
{
	_IN,
	_OUT,
	_NONE
};

enum actionType
{
	_INSERT,
	_UPDATE,
	_DELETE
};
struct KeyItem
{
	workTimeType wType;
	checkType cType;
	KeyItem()
	{
		wType = NONE;
		cType = _NONE;
	}
	KeyItem(workTimeType wt, checkType ct)
	{
		wType = wt;
		cType = ct;
	}
	bool operator==(const KeyItem& other) const
	{
		return wType == other.wType && cType == other.cType;
	}
};
struct Item
{
	static int id;
	static int verCode;
	static CString sensorID;
	static CString workCode;
	static CString sn;
	static int userExtFmt;
	CString cTime;
	SYSTEMTIME time = {};
	bool isOk = false;
	actionType aType = _INSERT;
	//CString sql;
};

struct _Item_hash
{	// hash functor for plain old data
	size_t operator()(const KeyItem& _Keyval) const
	{	// hash _Keyval to size_t value by pseudorandomizing transform
		char ch[2] = { _Keyval.wType * 100, _Keyval.cType * 5 };
		return (_Hash_seq((const unsigned char *)ch, sizeof(ch)));
	}
};

struct DateItem
{	// hash functor for plain old data
	size_t operator()(const DateItem& _Keyval) const
	{	// hash _Keyval to size_t value by pseudorandomizing transform

		char data[128];
		sprintf_s(data, "%02d%02d", _Keyval.data.tm_mon, _Keyval.data.tm_mday);
		return (_Hash_seq((const unsigned char *)data, strlen(data)));
	}
	bool operator()(const DateItem& _Left, const DateItem& _Right) const
	{	// apply operator== to operands
		//return _Left.data.tm_year == _Right.data.tm_year && _Left.data.tm_mon == _Right.data.tm_mon && _Left.data.tm_mday == _Right.data.tm_mday;
		return  _Left.data.tm_mon == _Right.data.tm_mon && _Left.data.tm_mday == _Right.data.tm_mday;
	}
	tm data;
	char ampm;
};

struct DayItem
{
	bool isOk;
	int day;
	unordered_map<KeyItem, Item, _Item_hash> items;
	static int month;
	static int year;
	DayItem(int type = 0)
	{
		switch (type)
		{
		case 'A':
			items = unordered_map<KeyItem, Item, _Item_hash>({ { KeyItem(AM, _IN), {} }, { KeyItem(AM, _OUT), {} } });
			break;
		case 'P':
			items = unordered_map<KeyItem, Item, _Item_hash>({ { KeyItem(PM, _IN), {} }, { KeyItem(PM, _OUT), {} } });
			break;
		default:
			items = unordered_map<KeyItem, Item, _Item_hash>({ { KeyItem(AM, _IN), {} }, { KeyItem(AM, _OUT), {} }, { KeyItem(PM, _IN), {} }, { KeyItem(PM, _OUT), {} } });
			break;
		}
		isOk = false;
		day = -1;
	}
	CString checkTime(workTimeType wT, checkType cT)
	{
		KeyItem tmp(wT, cT);
		auto iter = items.find(tmp);
		CString sql;
		if (iter != items.end())
		{
			pair<time_t, time_t> sTime = sWorkTime[wT][cT];
			time_t time = mktime(iter->second.time.wHour, iter->second.time.wMinute, iter->second.time.wSecond);
			iter->second.isOk = time > sTime.first && time < sTime.second;
			if (!iter->second.isOk)
			{
				time_t duration = sTime.second - sTime.first;
				time = sTime.first + rand() % duration;
				iter->second.time.wHour = time / 3600;
				iter->second.time.wMinute = (time - iter->second.time.wHour * 3600) / 60;
				iter->second.time.wSecond = (time - iter->second.time.wHour * 3600) - iter->second.time.wMinute * 60;
				cout << day << "号 ";
				switch (wT)
				{
				case AM:
					cout << "早上 ";
					break;
				case PM:
					cout << "下午 ";
					break;
				}

				switch (cT)
				{
				case _IN:
					cout << "上班 ";
					break;
				case _OUT:
					cout << "下班 ";
					break;
				}
				if (iter->second.cTime.GetLength())
				{
					wprintf(_T("%s"), &(iter->second.cTime.GetBuffer()[11]));
				}
				else
					cout << "未签到";
				cout << " 将修改时间到 " << iter->second.time.wHour << ":" << iter->second.time.wMinute << ":" << iter->second.time.wSecond << endl;
				SYSTEMTIME  nTm = iter->second.time;
				switch (iter->second.aType)
				{
				case _INSERT:
					sql.Format(_T("INSERT INTO CHECKINOUT (USERID, CHECKTIME, CHECKTYPE, VERIFYCODE, SENSORID, WorkCode, sn, UserExtFmt)  VALUES ('%d', '%4d-%02d-%02d %02d:%02d:%02d', '%c', '%d', '%s', '%s', '%s', '%d')"),
						Item::id, DayItem::year, DayItem::month, day, nTm.wHour, nTm.wMinute, nTm.wSecond, cT ? 'O' : 'I', \
						Item::verCode, Item::sensorID.GetBuffer(), Item::workCode.GetBuffer(), \
						Item::sn.GetBuffer(), Item::userExtFmt);
					break;
				case _UPDATE:
					sql.Format(_T("UPDATE CHECKINOUT SET CHECKTIME='%4d-%02d-%02d %02d:%02d:%02d' where USERID=%d AND [CHECKTIME]=CDate('%s')"), \
						nTm.wYear, nTm.wMonth, day, nTm.wHour, nTm.wMinute, nTm.wSecond, Item::id, iter->second.cTime);
					break;
				}
			}
		}
		return sql;
	}
};
int DayItem::month = 0;
int DayItem::year = 0;
int Item::id;
int Item::verCode = 1;
CString Item::sensorID = "1";
CString Item::workCode = "0";
CString Item::sn = "3084150200011";
int Item::userExtFmt = 1;

pair<time_t, time_t> sWorkTime[2][2];
typedef unordered_set<DateItem, DateItem, DateItem> DateSet;
void readDateFromFile(const CHAR* file, DateSet& set)
{
	FILE* testInput;
	fopen_s(&testInput, file, "r");
	if (testInput == NULL)
	{
		return;
	}
	char buffer[2048] = {};
	while (fgets(buffer, sizeof(buffer), testInput))
	{
		tm tmp = {};
		char ampm = 0;
		sscanf_s(buffer, "%2d%2d", &tmp.tm_mon, &tmp.tm_mday);
		if (strlen(buffer) > 4)
		{
			sscanf_s(buffer, "%*2d%*2d%c", &ampm, sizeof ampm);
			if (!isalpha(ampm))
			{
				ampm = 0;
			}
		}
		--tmp.tm_mon;
		DateItem item = { tmp, ampm };
		set.insert(item);
	}
	fclose(testInput);
}



int _tmain(int argc, _TCHAR* argv[])
{
	_wsetlocale(LC_CTYPE, L"chs");
	srand(time(NULL));

	sWorkTime[AM][_IN].first = mktime(7, 50, 0);
	sWorkTime[AM][_IN].second = mktime(8, 30, 0);
	sWorkTime[AM][_OUT].first = mktime(11, 30, 0);
	sWorkTime[AM][_OUT].second = mktime(12, 00, 0);
	sWorkTime[PM][_IN].first = mktime(12, 10, 0);
	sWorkTime[PM][_IN].second = mktime(12, 30, 0);
	sWorkTime[PM][_OUT].first = mktime(16, 30, 0);
	sWorkTime[PM][_OUT].second = mktime(17, 00, 0);

	SYSTEMTIME tm;
	GetLocalTime(&tm);
	//shortTime sTime;
	//sTime.mkShortTime(tm);
	DAOObject dao(_T("att2000.mdb"));
	DateSet work, holiday;
	readDateFromFile("work.txt", work);
	readDateFromFile("holiday.txt", holiday);
	try
	{
		CheckAttendance ca(&dao, argv[1]);
		cout << "请输入检查年（回车默认今年）：";
		int c = std::cin.peek();
		if (c != EOF && c != '\n')
			cin >> tm.wYear;
		cout << "请输入检查月份：";
		cin >> DayItem::month;
		CString sql;
		DayItem::year = tm.wYear;
		struct tm  target = {}, when = {};
		time_t  result, t;
		when.tm_year = tm.wYear - 1900;
		when.tm_mday = 1;
		target = when;
		when.tm_mon = (DayItem::month) % 12;
		if (when.tm_mon < DayItem::month - 1)
			++when.tm_year;
		if ((result = mktime(&when)) == (time_t)-1)
		{
			perror("mktime failed");
			throw 1;
		}
		target.tm_mon = (DayItem::month - 1) % 12;
		vector<DayItem> workDays(31);
		for (; (t = mktime(&target)) != (time_t)-1 && t < result; target.tm_mday++)
		{
			DateItem dTmp = { target };
			DateSet::iterator iterW = work.end(), iterH = holiday.find(dTmp);
			bool isHoliday = iterH != holiday.end() ? iterH->ampm == 0 : false;
			if (!isHoliday && (target.tm_wday > 0 && target.tm_wday < 6 || (iterW = work.find(dTmp)) != work.end()))
			{
				if (target.tm_mday >= workDays.size())
				{
					workDays.resize(target.tm_mday + 1);
				}
				if (iterW != work.end())
				{
					dTmp.ampm = iterW->ampm;
				}
				if (iterH != holiday.end() && iterH->ampm != 0)
				{
					dTmp.ampm = iterH->ampm == 'A' ? 'P' : 'A';
				}
				DayItem dItem(dTmp.ampm);
				dItem.day = target.tm_mday;
				workDays[target.tm_mday] = dItem;
			}
		}
		Item::id = ca.getID();
		sql.Format(_T("SELECT * from CHECKINOUT where USERID = %d and CHECKTIME  between  #%4d-%d-%d# and  #%4d-%d-%d# ORDER BY CHECKTIME"), ca.getID(), tm.wYear, DayItem::month, 1, when.tm_year + 1900, when.tm_mon + 1, 1);
		CheckHelper helper;
		helper.eachRow = [&workDays](DAOObject * dao, CDaoRecordset& set){
			COleVariant var;
			set.GetFieldValue(_T("CHECKTIME"), var);
			DATE date = var.date;
			SYSTEMTIME st;
			VariantTimeToSystemTime(date, &st);
			DayItem* dItem = &workDays[st.wDay];
			if (dItem->day != -1)
			{
				if (Item::verCode == -1)
				{
					set.GetFieldValue(_T("VERIFYCODE"), var);
					Item::verCode = var.intVal;
				}
				if (Item::userExtFmt == -1)
				{
					set.GetFieldValue(_T("UserExtFmt"), var);
					Item::userExtFmt = var.intVal;
				}
				if (Item::sensorID.IsEmpty())
				{
					set.GetFieldValue(_T("SENSORID"), var);
					Item::sensorID = buildStr(var.bstrVal).c_str();
				}
				if (Item::workCode.IsEmpty())
				{
					set.GetFieldValue(_T("WorkCode"), var);
					Item::workCode = buildStr(var.bstrVal).c_str();
				}
				if (Item::sn.IsEmpty())
				{
					set.GetFieldValue(_T("sn"), var);
					Item::sn = buildStr(var.bstrVal).c_str();
				}

				set.GetFieldValue(_T("CHECKTYPE"), var);
				BSTR type = var.bstrVal;
				checkType cType = _NONE;
				workTimeType wType = NONE;
				if (type)
				{
					if (type[0] == 'I')
						cType = _IN;
					else if (type[0] == 'O')
						cType = _OUT;
					else
						int i = 1;
				}
				if (st.wHour < 12 && st.wHour >= 0)
				{
					wType = AM;
				}
				else
				{
					wType = PM;
				}
				KeyItem tmp(wType, cType);
				auto iter = dItem->items.find(tmp);
				if (iter != dItem->items.end())
				{
					CString strTime;
					strTime.Format(_T("%04d-%02d-%02d %02d:%02d:%02d"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
					if (iter->second.aType == _INSERT)
					{
						iter->second.cTime = strTime;
						iter->second.time = st;
						iter->second.aType = _UPDATE;
					}
					else
					{
						function<bool(CString time1, CString time2)> fun;
						if ((wType == AM && cType == _IN) || (wType == PM && cType == _IN))
						{
							fun = [](CString& time1, CString& time2){
								return time1 > time2;
							};
						}
						else if ((wType == AM && cType == _OUT) || (wType == PM && cType == _OUT))
						{
							fun = [](CString& time1, CString& time2){
								return time1 < time2;
							};
						}
						if (fun && fun(iter->second.cTime, strTime))
						{
							iter->second.time = st;
							iter->second.cTime = strTime;
						}
					}
				}
			}
		};

		helper.finish = [&workDays](DAOObject * dao){
			for (size_t i = 1; i < workDays.size(); ++i)
			{
				DayItem* dItem = &workDays[i];
				if (dItem->day == -1)
				{
					continue;
				}
				auto fun = [dao](const CString& sql)
				{
					if (sql.GetLength() > 0)
						dao->excute(sql);
				};
				CString sql = dItem->checkTime(AM, _IN);
				fun(sql);
				sql = dItem->checkTime(AM, _OUT);
				fun(sql);
				sql = dItem->checkTime(PM, _IN);
				fun(sql);
				sql = dItem->checkTime(PM, _OUT);
				fun(sql);
			}
			//dItem->isOk = true;
		};
		ca.checkDate(sql.GetBuffer(), &helper);
	}
	catch (...)
	{

	}


	//DAOObject::selRet ret;

	//dao.select(_T("SELECT * FROM CHECKINOUT"), ret);
	//dao.select(_T("SELECT * FROM CHECKINOUT where CHECKTIME > #2015-2-26#"), ret);
	//LPCSTR wd = "夏雨";
	//cout << wd << endl;
	//wprintf(L"%s", argv[1]);
	////char buf[128];
	////cin >> buf;
	//BSTR bstr = ret[30][3].var.bstrVal;
	//string s = buildStr(bstr);
	//CString str = s.c_str();
	//LPCWSTR data = str.GetBuffer();
	//////ConvertGBKToUtf8(str);
	////Convert(str, 936, CP_UTF8);
	//if (str == argv[1])
	//{
	//	cout << "HAH" << endl;
	//}
	AfxDaoTerm();
	return 0;
}

