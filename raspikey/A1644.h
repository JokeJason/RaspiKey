//
// RaspiKey Copyright (c) 2019 George Samartzidis <samartzidis@gmail.com>. All rights reserved.
// You are not allowed to redistribute, modify or sell any part of this file in either 
// compiled or non-compiled form without the author's written permission.
//

#pragma once

#include <cstdint>
#include <cstddef>
#include "ReportFilter.h"
#include "GenericReportFilter.h"

typedef struct A1644HidReport
{
	uint8_t ReportId;
	uint8_t Modifier;
	uint8_t Reserved;
	uint8_t Key1;
	uint8_t Key2;
	uint8_t Key3;
	uint8_t Key4;
	uint8_t Key5;
	uint8_t Key6;
	uint8_t Special;
} tagA1644HidReport;

typedef struct A1644Settings
{
	bool SwapFnCtrl = true;
	bool SwapAltCmd = false;	
} tagA1644Settings;

class A1644 : public IReportFilter
{
public:
	A1644();
	virtual ~A1644();

private:
	bool m_FakeFnActive = false;
	bool m_MultimediaKeyActive = false;	

protected:
	A1644Settings m_Settings;

public:	
	size_t ProcessInputReport(uint8_t* buf, size_t len) override;
	size_t ProcessOutputReport(uint8_t* buf, size_t len) override;

	std::string GetSettings() override;
	void SetSettings(std::string settings) override;
};

