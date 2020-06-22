//
// RaspiKey Copyright (c) 2019 George Samartzidis <samartzidis@gmail.com>. All rights reserved.
// You are not allowed to redistribute, modify or sell any part of this file in either 
// compiled or non-compiled form without the author's written permission.
//

#include "Globals.h"
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <functional>
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <sys/sysinfo.h>
#include <algorithm>
#include "Logger.h"
#include <regex>
#include "json.hpp"


using namespace std;

namespace Globals
{	
	char g_szModuleDir[PATH_MAX];

	string FormatBuffer(const uint8_t* const buf, size_t len)
	{
		ostringstream ss;
		for (size_t i = 0; i < len; i++)
			ss << std::hex << (short)buf[i] << " ";
		ss << std::flush;

		return ss.str();
	}

	vector<string> Split(const string& s, char seperator)
	{
		vector<string> output;
		string::size_type prev_pos = 0, pos = 0;

		while ((pos = s.find(seperator, pos)) != string::npos)
		{
			string substring(s.substr(prev_pos, pos - prev_pos));
			output.push_back(substring);
			prev_pos = ++pos;
		}
		output.push_back(s.substr(prev_pos, pos - prev_pos)); //Last token

		return output;
	}

	string& Ltrim(string &s)
	{
		s.erase(s.begin(), find_if(s.begin(), s.end(), not1(ptr_fun<int, int>(isspace))));
		return s;
	}

	string& Rtrim(string &s)
	{
		s.erase(find_if(s.rbegin(), s.rend(), not1(ptr_fun<int, int>(isspace))).base(), s.end());
		return s;
	}

	string& Trim(string &s)
	{
		return Ltrim(Rtrim(s));
	}

	long GetUptime()
	{
		struct sysinfo s_info;
		int error = sysinfo(&s_info);
		if (error != 0)
			ErrorMsg("sysinfo error: %d", error);
		
		return s_info.uptime;
	}

	bool GetBtHidBatteryCapacity(const string& btaddress, int& capacity)
	{
		string btaddresslower = btaddress;
		transform(btaddresslower.begin(), btaddresslower.end(), btaddresslower.begin(), ::tolower);

		char pfad[128];
		snprintf(pfad, 128, "/sys/class/power_supply/hid-%s-battery/capacity", btaddresslower.c_str());
		FILE* f = fopen(pfad, "r");
		if (f == nullptr)
			return false;

		fscanf(f, "%d", &capacity);		
		fclose(f);

		return true;
	}

	bool SetPiLedState(const bool state, const char* led)
	{
		char pfad[128];
		snprintf(pfad, 128, "/sys/class/leds/%s/brightness", led);
		FILE* f = fopen(pfad, "w");
		if (f == nullptr) 
			return false;
		fprintf(f, "%i", state ? 1 : 0);
		fclose(f);

		return true;
	}

	bool FileReadAllText(const string& path, string& text)
	{
		ifstream keyMapFs(path);
		if (!keyMapFs.good())
			return false;

		try
		{
			stringstream ss;
			ss << keyMapFs.rdbuf();
			text = ss.str();
			keyMapFs.close();
		}
		catch (const exception& m)
		{
			ErrorMsg(m.what());
			return false;
		}

		return true;
	}

	bool FileWriteAllText(const string& path, const string& text)
	{
		ofstream ofs;
		ofs.open(path, fstream::out | fstream::trunc);
		if (!ofs.is_open())
			return false;
		
		ofs << text;
		ofs.close();

		return true;
	}

	bool DeleteFile(const string& path)
	{
		ifstream ifs(path);
		if (!ifs.good())
			return false;
		ifs.close();

		if (remove(path.c_str()) != 0)
			return false;

		return true;
	}		
}