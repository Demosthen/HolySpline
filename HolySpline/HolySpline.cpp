
#include "pch.h"
#include <windows.h>
#include "tinysplinecpp.h"
#include <iostream>
#include <fstream>
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int pxSz = 2;
int threshold = 10;//max distance in pixels for point selection instead of creation
int wheelDist = 30; //how many feet away are the wheels from the center of the bot
inline double Distance(POINT r1, POINT r2) {
	double xDiff = r1.x - r2.x;
	double yDiff = r1.y - r2.y;
	return sqrt(xDiff*xDiff + yDiff * yDiff);
}

struct StateInfo {
	POINT* cursorPos;
	HBITMAP bmp ;
	std::vector<POINT > controlPoints;
	std::vector<POINT > splinePoints;
	std::vector<POINT > leftWheelPoints;
	std::vector<POINT > rightWheelPoints;
	tinyspline::BSpline spline;
	tinyspline::BSpline deriv;
	bool holding = false;
	int index = 0;
	StateInfo() {
		cursorPos = new POINT;
	}
	
public:
	void ConstructSpline() {
		//tinyspline::BSpline temp = tinyspline::BSpline(3, 2, 7, tinyspline::BSpline::type::TS_CLAMPED);
		tinyspline::BSpline temp(controlPoints.size());
		temp.setControlPoints(PointToTinyReal(controlPoints));
		spline = temp;
	}
	inline POINT TinyRealToPoint(std::vector<tinyspline::real> tiny) {
		POINT a;
		a.x = tiny[0];
		a.y = tiny[1];
		return a;
	}
	inline std::vector<tinyspline::real> PointToTinyReal(std::vector<POINT> &pts) {
		std::vector<tinyspline::real> a;
		a.resize(2*pts.size());
		for (int i = 0; i < pts.size(); i++) {
			a[2 * i] = pts[i].x;
			a[2 * i + 1] = pts[i].y;
		}
		return a;
	}
	void PopulateSpline(float stepSize = 0.001) {
		splinePoints.clear();
		splinePoints.reserve(int( 1 / (stepSize)));
		for (float i = 0; i < 1; i+=stepSize) {
			auto current = spline.eval(i).result();
			splinePoints.push_back(TinyRealToPoint(current));
		}
	}
	double GetArcLength(std::vector<POINT > &pts, float stepSize = 0.001 ) {
		double len = 0;
		for (int i = 1; i < pts.size(); i++) {
			len += Distance(pts[i], pts[i-1]);
		}
		return len;
	}
	std::vector<double> GetArcLengths(std::vector<POINT> &pts) {//test this when you get home
		std::vector<double> ans;
		double len = 0;
		ans.reserve(pts.size());
		ans.push_back(0);
		for (int i = 1; i < pts.size(); i++) {
			len += Distance(pts[i], pts[i - 1]);
			ans.push_back(len);
		}
		return ans;
	}
	void Derive() {
		deriv = spline.derive(1);
	}
	void CreateWheelPaths(float stepSize = 0.001) {
		leftWheelPoints.resize(splinePoints.size());
		rightWheelPoints.resize(splinePoints.size());
		Derive();
		for (int i = 0; i < splinePoints.size(); i++) {
			auto parSlope = deriv(i * stepSize).result();
			
			double hypotenuse = std::sqrt(parSlope[0] * parSlope[0] + parSlope[1] * parSlope[1]);
			double scale = wheelDist / hypotenuse;
			leftWheelPoints[i].x = splinePoints[i].x - parSlope[1] * scale;
			leftWheelPoints[i].y = splinePoints[i].y + parSlope[0] * scale;
			rightWheelPoints[i].x = splinePoints[i].x + parSlope[1] * scale;
			rightWheelPoints[i].y = splinePoints[i].y - parSlope[0] * scale;

		}
	}
	double GetAngle(float u) {
		
		auto parSlope = deriv(u).result();
		return atan2(parSlope[1],parSlope[0]);//returns radians in interval [-pi, pi]
	}
	void Record(std::string path) {
		std::ofstream fout(path);
		fout << splinePoints.size() << std::endl;
		std::vector<double> leftLengths = GetArcLengths(leftWheelPoints);
		std::vector<double> rightLengths = GetArcLengths(rightWheelPoints);
		for (int i = 0; i < splinePoints.size(); i++) {
			fout << leftWheelPoints[i].x << " " << leftWheelPoints[i].y <<" "<<leftLengths[i]<< std::endl;
		}
		for (int i = 0; i < splinePoints.size(); i++) {
			fout << rightWheelPoints[i].x << " " << rightWheelPoints[i].y <<" "<<rightLengths[i]<< std::endl;
		}
	}
	~StateInfo() {
		delete cursorPos;
	}

};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
	// Register the window class.
	const wchar_t CLASS_NAME[] = L"Sample Window Class";

	WNDCLASS wc = { };

	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	StateInfo* state = new StateInfo;
	// Create the window.

	HWND hwnd = CreateWindowEx(
		0,                              // Optional window styles.
		CLASS_NAME,                     // Window class
		L"Learn to Program Windows",    // Window text
		WS_OVERLAPPEDWINDOW,            // Window style

		// Size and position
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

		NULL,       // Parent window    
		NULL,       // Menu
		hInstance,  // Instance handle
		state        // Additional application data
	);

	if (hwnd == NULL)
	{
		return 0;
	}

	ShowWindow(hwnd, nCmdShow);

	// Run the message loop.

	MSG msg = { };
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		CREATESTRUCT *create = reinterpret_cast<CREATESTRUCT*>(lParam);
		StateInfo *state = reinterpret_cast<StateInfo*>(create->lpCreateParams);

		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
		auto path = L"C:\\Users\\10089\\Pictures\\four.bmp";
		auto inst = reinterpret_cast<HINSTANCE*>(GetWindowLongPtr(hwnd, -6));
		state->bmp = (HBITMAP)LoadImage(*inst, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		if (state->bmp == NULL) {
			auto a = GetLastError();
		}
		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_CLOSE:
		if (MessageBox(hwnd, L"Really quit?", L"My application", MB_OKCANCEL) == IDOK)
		{
			DestroyWindow(hwnd);
		}
		// Else: User canceled. Do nothing.
		return 0;

	case WM_LBUTTONDOWN:
	{
		StateInfo * state = reinterpret_cast<StateInfo *> (GetWindowLongPtr(hwnd, GWLP_USERDATA));
		POINT * pos = state->cursorPos;
		POINT origin;
		origin.x = 0;
		origin.y = 0;
		ClientToScreen(hwnd, &origin);
		if (GetCursorPos(pos)) {
			POINT correctedPos;
			correctedPos.x = pos->x - origin.x;
			correctedPos.y = pos->y - origin.y;
			for (int i = 0; i < state->controlPoints.size(); i++) {
				if (Distance(correctedPos, state->controlPoints[i]) <= threshold) {
					state->holding = true;
					state->index = i;
					RECT update;
					update.left = max(state->controlPoints[i].x - pxSz, 0);
					update.right = state->controlPoints[i].x + pxSz;
					update.bottom = state->controlPoints[i].y + pxSz;
					update.top = max(state->controlPoints[i].y - pxSz, 0);
					InvalidateRect(hwnd, &update, FALSE);
					state->controlPoints.erase(state->controlPoints.begin() + i);
					break;
				}
			}
			if (!state->holding) {
				std::wstring s1 = L"Your cursor is at x position: " + std::to_wstring(correctedPos.x) + L" and y position: " + std::to_wstring(correctedPos.y) + L". Place point here?";

				if (MessageBox(hwnd, s1.c_str(), L"My application", MB_OKCANCEL) == IDOK) {
					state->controlPoints.push_back(correctedPos);
					RECT update;
					update.left = max(correctedPos.x - pxSz, 0);
					update.right = correctedPos.x + pxSz;
					update.bottom = correctedPos.y + pxSz;
					update.top = max(correctedPos.y - pxSz, 0);
					InvalidateRect(hwnd, &update, FALSE);
				}
			}
		}
		return 0;
	}
	case WM_LBUTTONUP:
	{
		StateInfo* state = reinterpret_cast<StateInfo*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		POINT * pos = state->cursorPos;
		POINT origin;
		origin.x = 0;
		origin.y = 0;
		ClientToScreen(hwnd, &origin);
		if (GetCursorPos(pos)) {
			POINT correctedPos;
			correctedPos.x = pos->x - origin.x;
			correctedPos.y = pos->y - origin.y;
			if (state->holding) {
				state->holding = false;
				state->controlPoints.insert(state->controlPoints.begin() + state->index, correctedPos);
				RECT update;
				update.left = max(correctedPos.x - pxSz, 0);
				update.right = correctedPos.x + pxSz;
				update.bottom = correctedPos.y + pxSz;
				update.top = max(correctedPos.y - pxSz, 0);
				InvalidateRect(hwnd, &update,FALSE);
			}
		}
		return 0;
	}
	case WM_RBUTTONDOWN:
	{
		std::wstring s1 = L"Would you like to construct the spline now?";
		if (MessageBox(hwnd, s1.c_str(), L"My application", MB_OKCANCEL) == IDOK) {
			StateInfo * state = reinterpret_cast<StateInfo*> (GetWindowLongPtr(hwnd, GWLP_USERDATA));
			state->ConstructSpline();
			state->PopulateSpline();
			state->CreateWheelPaths();
			InvalidateRect(hwnd, NULL,TRUE);
			UpdateWindow(hwnd);
			//MessageBox(hwnd, (std::to_wstring(reinterpret_cast<StateInfo*> (GetWindowLongPtr(hwnd, GWLP_USERDATA))->GetArcLength())).c_str(), L"My Application", MB_OKCANCEL);
			return 0;
		}
		
		return 0;
	}

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		//draw field map
		BITMAP bitmap;
		HGDIOBJ oldBitmap;
		HDC hdcMem;
		HDC hdc = BeginPaint(hwnd, &ps);
		HBITMAP hBitmap = reinterpret_cast<StateInfo*> (GetWindowLongPtr(hwnd, GWLP_USERDATA))->bmp;
		hdcMem = CreateCompatibleDC(hdc);
		oldBitmap = SelectObject(hdcMem,hBitmap);
		GetObject(hBitmap, sizeof(BITMAP), &bitmap); //writes image from hBitmap to bitmap
		BitBlt(hdc, 0, 0, bitmap.bmWidth, bitmap.bmHeight, hdcMem, 0, 0, SRCCOPY);
		SelectObject(hdcMem, oldBitmap);
		DeleteDC(hdcMem);
		//draw points
		StateInfo* state = reinterpret_cast<StateInfo*> (GetWindowLongPtr(hwnd, GWLP_USERDATA));
		int sz = state->controlPoints.size();
		for (int i = 0; i < sz; i++) {
			int x = state->controlPoints[i].x;
			int y = state->controlPoints[i].y;
			Ellipse(hdc, max(x - pxSz,0), max(y - pxSz,0), x + pxSz, y + pxSz);
		}
		int splineSz = state->splinePoints.size();
		if(splineSz>0)
			Polyline(hdc, &state->splinePoints[0],splineSz);
			if (state->leftWheelPoints.size() > 0) {
				Polyline(hdc, &state->leftWheelPoints[0], splineSz);
				Polyline(hdc, &state->rightWheelPoints[0], splineSz);
			}
		EndPaint(hwnd, &ps);
		return 0;
	}
	
	}
	
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/*
TODO:

*/