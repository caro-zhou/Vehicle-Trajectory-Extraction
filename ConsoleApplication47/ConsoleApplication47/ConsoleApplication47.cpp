#include "pch.h"
#include <opencv2/video/background_segm.hpp>  //重要！！
#include <iostream>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <vector>
#include <string>
#include <opencv2/core/utility.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <cstring>
#include <ctime>
#include <math.h>

using namespace std;
using namespace cv;

bool select_flag = false;
Mat img, showImg;
Point P1, P2;       //输入交叉口分隔线的两个端点
Point ppp;
vector<String> line_name = { "西","南","东","北" };
//vector<int> sm_veh(12);
//vector<int> lg_veh(12);
int NW_s, NS_s, NE_s, WN_s, WS_s, WE_s, SN_s, SW_s, SE_s, EN_s, EW_s, ES_s;     //各个方向轨迹的车辆数（小车）
int NW_l, NS_l, NE_l, WN_l, WS_l, WE_l, SN_l, SW_l, SE_l, EN_l, EW_l, ES_l;     //各个方向轨迹的车辆数（大车）
float img_len;   //图中线段像素长度
float scale;     //比例尺

int end_AAA = 3000;     //结束帧数
int frameNum = 3100;   //总帧数  require:frameNum>end_AAA
//跟踪相关
int veh_num = 400;    //预计车辆数量

//vector<Rect> Record_rt(200);    //记下每帧的rt
//vector<Rect> Record_rt2(200);    //记下每帧的rt2
//vector<vector<Point>>Record_cp(frameNum);   //记下每辆车中心位置
vector <vector<Rect>> Record_ROI(frameNum);      //记录每帧ROI，第一维为帧数，第二维为车号
vector <vector<Rect>> Track(veh_num);  //暂定共1000辆车，一维车辆号，二维帧数
vector <vector<int>> Track_app_frame(veh_num);      //记下车号对应出现的帧号
vector<int> name;     //车号
vector<Point> lostP;        //跟丢的车辆矩心
vector<int> lostN;           //跟丢的车辆名
vector<int> losttime;       //跟丢的帧数
vector<vector<Point>> contours;     //车辆轮廓向量 第一维-车辆；第二维-车辆轮廓的一组点
vector<Rect> rt;   //当前帧的boundingrect的向量
vector<Rect2d> ROI;     //储存初始ROI

class vehicle
{
public:
	int name;     //车的编号
	String direct;    //车的方向
	float veh_size;   //车的像素大小
	bool sm_veh;   //是否为小车，true为小车
	float dur_time;    //交叉口通过时间，帧数/25
	float ave_spd;      //车的均速
	void init(int _name, String _direct, float _veh_size, bool _sm_veh, float _dur_time, float _ave_spd); //成员函数，初始化

};

void vehicle::init(int _name, String _direct, float _veh_size, bool _sm_veh, float _dur_time, float _ave_spd)
{
	name = _name; direct = _direct; sm_veh = _sm_veh; dur_time = _dur_time; ave_spd = _ave_spd; veh_size = _veh_size;
}

vehicle veh_set[300];      //车辆集，储存有用车辆的信息
int veh_count = 0;     //用于计数，车辆

class parting_line
{
public:
	float a;   //x的系数
	float b;   //y的系数
	float c;   //常数
	parting_line(float _a, float _b, float _c) :a(_a), b(_b), c(_c) {};
	parting_line() {};
	void init(float _a, float _b, float _c);
	//  line: ax+by+c=0
};

void parting_line::init(float _a, float _b, float _c)
{
	a = _a; b = _b; c = _c;
}

parting_line area_line[4];     //LW,LS,LE,LN
parting_line W_lane[10];
parting_line S_lane[10];
parting_line E_lane[10];
parting_line N_lane[10];

//W_front, W1,W2,W3,W4
//parting_line W_lane[5] = { parting_line(-1.39344, 1, 208.557),parting_line(0.615385,1,-598.154),
//parting_line(0.616601,1,-632.356),parting_line(0.621005,1,-664.516),parting_line(0.63354,1,-727.329) };
//parting_line S_lane[5] = { parting_line(0.666667, 1, -815),parting_line(-1.18605,1,387.744),
//parting_line(-1.17266,1,414.338),parting_line(-1.16667,1,443.333),parting_line(-1.1087,1,493.217) };
//parting_line E_lane[5] = { parting_line(-1.09091, 1, 556.818),parting_line(0.618056,1,-605.896),
//parting_line(0.605263,1,-568.289),parting_line(0.599174,1,-539.264),parting_line(0.588235,1,-488.471) };
//parting_line N_lane[6] = { parting_line(0.552941, 1, -411.882),parting_line(-1.15625,1,392.594),
//parting_line(-1.19084,1,377.26),parting_line(-1.24812,1,365.94),parting_line(-1.28148,1,347.533), parting_line(-1.28788,1,308.561) };

//parting_line W_lane[5] = { parting_line(-1.39344, 1, 208.557),parting_line(0.608541,1,-603.676),
//parting_line(0.605578,1,-642.582),parting_line(0.616114,1,-677.938),parting_line(0.620968,1,-762.168) };
//parting_line S_lane[5] = { parting_line(0.666667, 1, -815),parting_line(-1.18605,1,387.744),
//parting_line(-1.17266,1,414.338),parting_line(-1.1958,1,470.888),parting_line(-1.14634,1,493.537) };
//parting_line E_lane[5] = { parting_line(-1.09821, 1, 663.786),parting_line(0.608108,1,-613.987),
//parting_line(0.597701,1,-576.057),parting_line(0.599174,1,-539.264),parting_line(0.588235,1,-488.471) };
//parting_line N_lane[6] = { parting_line(0.552941, 1, -411.882),parting_line(-1.20635,1,429.873),
//parting_line(-1.2381,1,405.286),parting_line(-1.264,1,384.688),parting_line(-1.28148,1,347.533), parting_line(-1.28788,1,308.561) };

parting_line cal_line(Point A, Point B)
{
	parting_line myline;
	float x1 = float(A.x);
	float y1 = float(A.y);
	float x2 = float(B.x);
	float y2 = float(B.y);
	if (A.x != B.x)
		myline.init((y1 - y2) / (x2 - x1), 1, (x1 * y2 - x2 * y1) / (x2 - x1));
	else
		myline.init(1, 0, -x1);
	return myline;
}

void A_on_Mouse(int event, int x, int y, int flags, void*param)     //实现画点
{
	if (event == EVENT_LBUTTONDOWN)
	{
		P1 = Point(x, y);
		select_flag = true;
	}
	else if (select_flag &&event == EVENT_MOUSEMOVE)
	{
		img.copyTo(showImg);
		P2 = Point(x, y);
		line(showImg, P1, P2, Scalar(0, 255, 0), 2);
		imshow("img", showImg);
	}
	else if (select_flag && event == EVENT_LBUTTONUP)
	{
		select_flag = false;
	}
}    //用鼠标得到矩形

void C_on_Mouse(int event, int x, int y, int flags, void*param)     //实现画直线
{
	Point p;
	if (event == EVENT_LBUTTONDOWN)
	{
		ppp = Point(x, y);
		select_flag = true;
	}
	else if (select_flag &&event == EVENT_MOUSEMOVE)
	{
		img.copyTo(showImg);
		p = Point(x, y);
		line(showImg, ppp, p, Scalar(0, 255, 0), 2);
		int xx = pow((ppp.x - p.x), 2);
		int yy = pow((ppp.y - p.y), 2);
		img_len = sqrt(xx + yy);	
		imshow("img", showImg);
	}
	else if (select_flag && event == EVENT_LBUTTONUP)
	{
		cout << "img_len = " << img_len << "像素\n";
		select_flag = false;
	}
		
}    //用鼠标得到矩形

void get_roi(int i)
{
	cout << "请在图像中画出"<< line_name [i]<<"车道线，不满意可重画，若完成则输入Q...\n";
	while (1)
	{
		int key = waitKey(10);
		setMouseCallback("img", A_on_Mouse, 0);
		if (key == 27 || key == 'q' || key == 'Q')
		{
			cout << "输入成功!" << "\n";
			break;
		}
	}
}

void get_scale()
{
	cout << "请在图像中绘制一条线段，不满意可重画，若完成则输入Q...\n";
	while (1)
	{
		int key = waitKey(10);
		setMouseCallback("img", C_on_Mouse, 0);
		if (key == 27 || key == 'q' || key == 'Q')
		{
			cout << "输入成功!" << "\n";
			break;
		}
	}
	int act_len;    //实际长度
	cout << "请输入线段对应的实际长度(m)...\n";
	cin >> act_len;
	scale = act_len / img_len;
	cout << "比例为：图像长度：实际长度 = 1：" << scale << "\n";
}

bool biggerSort(vector<Point> v1, vector<Point> v2)
{
	return contourArea(v1) > contourArea(v2);
}

//得到矩形中心点
Point getCenterPoint(Rect rect)
{
	Point cpt;
	cpt.x = rect.x + round(rect.width / 2.0);
	cpt.y = rect.y + round(rect.height / 2.0);
	return cpt;
}

bool isNearby(Rect &rc1, Rect &rc2)    //判断是否重叠
{
	Point cpt1 = getCenterPoint(rc1);
	Point cpt2 = getCenterPoint(rc2);
	int delta = 15;

	if (cpt1.x - delta < cpt2.x && cpt1.x + delta > cpt2.x &&
		cpt1.y - delta < cpt2.y && cpt1.y + delta > cpt2.y)
		return true;
	else
		return false;
}

bool isOverlap(Rect &rc1, Rect &rc2)    //判断是否重叠
{
	if (rc1.x + rc1.width > rc2.x &&
		rc2.x + rc2.width > rc1.x &&
		rc1.y + rc1.height > rc2.y &&
		rc2.y + rc2.height > rc1.y
		)
		return true;
	else
		return false;
}

static Scalar randomColor(RNG& rng)
{
	int icolor = (unsigned)rng;
	return Scalar(icolor & 255, (icolor >> 8) & 255, (icolor >> 16) & 255);
}

float overlapRatio(Rect A, Rect B)
{
	float colInt = min(A.x + A.width, B.x + B.width) - max(A.x, B.x);
	float rowInt = min(A.y + A.height, B.y + B.height) - max(A.y, B.y);
	float intersection = colInt * rowInt;
	if (A.area() < B.area())           //重叠率：按照小的矩形为分母
		return intersection / A.area();
	else
		return intersection / B.area();
}

void morph_ope(Mat &se1, Mat &se2, Mat &se3, Mat &foreground)
{	
	//对前景进行中值滤波、形态学膨胀操作，以去除伪目标和接连断开的小目标		
	medianBlur(foreground, foreground, 1);        //中值滤波
	//medianBlur(foreground, foreground, 1);        //中值滤波
	//morphologyEx(foreground, foreground, MORPH_DILATE, se2, Point(-1, -1), 1);   //膨胀
	//morphologyEx(foreground, foreground, MORPH_ERODE, se2, Point(-1, -1), 1);   //腐蚀
	//morphologyEx(foreground, foreground, MORPH_DILATE, se2, Point(-1, -1), 1);   //膨胀
	
	morphologyEx(foreground, foreground, CV_MOP_OPEN, se2, Point(-1, -1), 1);  //1次开操作
	morphologyEx(foreground, foreground, CV_MOP_CLOSE, se2, Point(-1, -1), 1);   //1次闭操作
	morphologyEx(foreground, foreground, MORPH_DILATE, se2, Point(-1, -1), 1);   //膨胀

}

bool judge_rect(Point p, Rect r)   //判断点P是否在r内
{
	if ((p.x >= r.x) && (p.x <= r.x + r.width) && (p.y >= r.y) && (p.y <= r.y + r.height))
		return true;
	else
		return false;
}

bool area_W(Point p)
{
	if ((area_line[0].a*p.x + p.y + area_line[0].c) > 0 and
		(area_line[1].a*p.x + p.y + area_line[1].c) < 0 and
		(area_line[3].a*p.x + p.y + area_line[3].c) > 0)
		return true;
	else
		return false;
}
bool area_S(Point p)
{
	if ((area_line[1].a*p.x + p.y + area_line[1].c) > 0 and
		(area_line[0].a*p.x + p.y + area_line[0].c) < 0 and
		(area_line[2].a*p.x + p.y + area_line[2].c) > 0)
		return true;
	else
		return false;
}
bool area_E(Point p)
{
	if ((area_line[2].a*p.x + p.y + area_line[2].c) < 0 and
		(area_line[1].a*p.x + p.y + area_line[1].c) < 0 and
		(area_line[3].a*p.x + p.y + area_line[3].c) > 0)
		return true;
	else
		return false;
}
bool area_N(Point p)
{
	if ((area_line[3].a*p.x + p.y + area_line[3].c) < 0 and
		(area_line[0].a*p.x + p.y + area_line[0].c) < 0 and
		(area_line[2].a*p.x + p.y + area_line[2].c) > 0)
		return true;
	else
		return false;
}

int judge_area(Point p)     //判断点在哪个进出口道
{
	//注意：交叉口所在位置不同影响结果
	//area_line:W,S,E,N
	if (area_W(p))  	return 1;    //W口
	else if (area_S(p))     return 2;    //S口
	else if (area_E(p))     return 3;    //E口
	else if (area_N(p))  	return 4;    //N口
	else return 5;    //无
}

vector<Rect> generate(Ptr<BackgroundSubtractorMOG2> &bgsubtractor, Mat &frame,Mat &middle, Mat &foreground, Mat &se1, Mat &se2, Mat &se3, Mat &bw,
	int &AAA, double min_veh_sz, int &veh_num, Rect bounding, vector<Ptr<Tracker>> &TrackerVec)
{
	//检测目标(其实是边训练边检测)，记录到foreground里
	bgsubtractor->apply(frame, foreground, 0.01);
	threshold(foreground, foreground, 127, 255, CV_THRESH_TOZERO);     //去除阴影
	//imshow("混合高斯检测前景", foreground);
	morph_ope(se1, se2, se3, foreground);     //形态学操作
	//imshow("形态学操作后", foreground);
	waitKey(30);

	//检索前景中各个连通分量的轮廓
	foreground.copyTo(bw);
	vector<vector<Point>> contours;	
	findContours(bw, contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);
	
	vector<vector<Point>>::iterator it_border2;
	for (it_border2 = contours.begin(); it_border2 != contours.end();)      //删去超出边界的
	{
		Point rt_pt = getCenterPoint(boundingRect(*it_border2));
		if (!judge_rect(rt_pt, bounding))
			it_border2 = contours.erase(it_border2);
		else
			++it_border2;
	}

	//对连通分量进行排序
	std::sort(contours.begin(), contours.end(), biggerSort);

	vector<Rect> gen;    //结果vector
	for (int k = 0; k < contours.size(); k++)  //对每一个轮廓,得到新的rt[k]
	{
		//第k个连通分量的外接矩形框
		if (contourArea(contours[k]) < max(contourArea(contours[0]) / 5, min_veh_sz))  //可改
			break;
		if (contourArea(contours[k]) > 2000)          //可接受车辆大小，可改
			break;
		Rect get = boundingRect(contours[k]);
		rt.push_back(get);	
	}
	cout << "rtsize" << rt.size() << endl;
	for (int i = 0; i < rt.size(); i++)
	{
		rectangle(foreground, rt[i], Scalar(255, 255, 255), 2);
	}

	vector<bool> HaveOverlap(rt.size(), false);
	for (int t = 0; t < rt.size(); t++)
	{
		//看是否和前一帧的rt重合
		for (int m = 0; m < ROI.size(); m++)     //Record_ROI[AAA - 1]
		{
			//Rect M = Rect(Record_ROI[AAA - 1][m]);     //前一帧m车
			Rect M = Rect(ROI[m]);
			//两个搜索框重叠且重叠率>80%
			if (isOverlap(rt[t], M) == true and overlapRatio(rt[t], M) > 0.3)
				//若新窗口大，则替换ROI，面积不应过大，以免粘连；去掉原来的框
			{
				/*if (rt[t].area() > M.area())
				{
					ROI[m] = rt[t];
					TrackerVec[m]->init(frame, ROI[m]);
					cout << "更改" << m << "车ROI大小" << '\n';
				}*/
				HaveOverlap[t] = true;
			}
		}
	}
	for (int t = 0; t < rt.size(); t++)
	{
		if (!HaveOverlap[t])
		{
			gen.push_back(rt[t]);
			cout << "生成gen" << gen.size();
		}	
	}

	cout << " rt.size()" << rt.size() << endl;
	rt.clear();

	/*for (int m = 0; m < gen.size(); m++)
		rectangle(middle, gen[m], Scalar(197, 95, 134), 2);*/

	imshow("rt示意图", foreground);
	//imshow("middle", middle);
	cv::waitKey(30);
	cout << "gen.size:" << gen.size() << endl;
	return gen;       //return是直接返回并关闭函数
}

void cal_dur_time(vector<Point> veh, int in, int out, int &in_time, int &out_time)
{
	switch (in)
	{
	case 1:
	{
		for (int n = 0; n < veh.size(); n++)
		{
			if (!area_W(veh[n]))
			{
				in_time = n;
				break;
			}
		}
		switch (out)
		{
		case 2:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_S(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 3:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_E(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 4:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_N(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 5:
		{
			out_time = veh.size();
		}
		}
		break;
	}
	case 2:
	{
		for (int n = 0; n < veh.size(); n++)
		{
			if (!area_S(veh[n]))
			{
				in_time = n;
				break;
			}
		}
		switch (out)
		{
		case 1:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_W(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 3:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_E(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 4:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_N(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 5:
			out_time = veh.size();
		}
		break;
	}
	case 3:
	{
		for (int n = 0; n < veh.size(); n++)
		{
			if (!area_E(veh[n]))
			{
				in_time = n;
				break;
			}
		}
		switch (out)
		{
		case 1:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_W(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 2:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_S(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 4:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_N(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 5:
			out_time = veh.size();
		}
		break;
	}
	case 4:
	{
		for (int n = 0; n < veh.size(); n++)
		{
			if (!area_N(veh[n]))
			{
				in_time = n;
				break;
			}
		}
		switch (out)
		{
		case 1:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_W(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 2:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_S(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 3:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_E(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 5:
			out_time = veh.size();
		}
		break; 
	}
	case 5:
	{
		in_time = 0;
		switch (out)
		{
		case 1:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_W(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 2:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_S(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 3:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_E(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 4:
		{
			for (int m = in_time; m < veh.size(); m++)
			{
				if (area_N(veh[m]))
				{
					out_time = m;
					break;
				}
			}
			break;
		}
		case 5:
			out_time = veh.size();
		}
	}
	}
}

void veh_result(Point PI, Point PO, vector<Point> veh, int ff, float veh_size, bool sm_veh, float ave_spd)
{
	//vector<Point> veh是车辆从头到尾的位置点集
	int in_time, out_time;
	int ii = judge_area(PI);
	int oo = judge_area(PO);
	switch (ii)
	{
	case 1:
	{
		switch (oo)
		{
		case 1:
		{
			float dur_time = veh.size() / 25.0;    //25帧/s，dur_time单位为s 
			veh_set[veh_count].init(ff, "WW", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 2:
		{
			if (sm_veh)  WS_s++;
			else   WS_l++;
			cal_dur_time(veh, 1, 2, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;    //25帧/s，dur_time单位为s
			veh_set[veh_count].init(ff, "WS", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 3:
		{
			if (sm_veh)  WE_s++;
			else  WE_l++;
			cal_dur_time(veh, 1, 3, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "WE", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 4:
		{
			if (sm_veh)  WN_s++;
			else  WN_l++;
			cal_dur_time(veh, 1, 4, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "WN", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 5:
		{
			cal_dur_time(veh, 1, 5, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "Wnone", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
		}
		}
		break;
	}
	case 2:
	{
	switch (oo)
	{
	case 1:
	{
		if (sm_veh)  SW_s++;
		else  SW_l++;
		cal_dur_time(veh, 2, 1, in_time, out_time);
		float dur_time = (out_time - in_time) / 25.0;
		veh_set[veh_count].init(ff, "SW", veh_size, sm_veh, dur_time, ave_spd);
		veh_count++;
		break;
	}
	case 2:
	{
		float dur_time = veh.size() / 25.0;    //25帧/s，dur_time单位为s
		veh_set[veh_count].init(ff, "SS", veh_size, sm_veh, dur_time, ave_spd);
		veh_count++;
		break;
	}
	case 3:
	{
		if (sm_veh)  SE_s++;
		else  SE_l++;
		cal_dur_time(veh, 2, 3, in_time, out_time);
		float dur_time = (out_time - in_time) / 25.0;
		veh_set[veh_count].init(ff, "SE", veh_size, sm_veh, dur_time, ave_spd);
		veh_count++;
		break;
	}
	case 4:
	{
		if (sm_veh)  SN_s++;
		else  SN_l++;
		cal_dur_time(veh, 2, 4, in_time, out_time);
		float dur_time = (out_time - in_time) / 25.0;
		veh_set[veh_count].init(ff, "SN", veh_size, sm_veh, dur_time, ave_spd);
		veh_count++;
		break;
	}
	case 5:
	{
		cal_dur_time(veh, 2, 5, in_time, out_time);
		float dur_time = (out_time - in_time) / 25.0;
		veh_set[veh_count].init(ff, "Snone", veh_size, sm_veh, dur_time, ave_spd);
		veh_count++;
		break;
	}
	}
	break;
	}
	case 3:
	{
		switch (oo) {
		case 1:
		{
			if (sm_veh)  EW_s++;
			else  EW_l++;
			cal_dur_time(veh, 3, 1, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "EW", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 2:
		{
			if (sm_veh)  ES_s++;
			else  ES_l++;
			cal_dur_time(veh, 3, 2, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "ES", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 3:
		{
			float dur_time = veh.size() / 25.0;    //25帧/s，dur_time单位为s
			veh_set[veh_count].init(ff, "EE", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 4:
		{
			if (sm_veh)  EN_s++;
			else  EN_l++;
			cal_dur_time(veh, 3, 4, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "EN", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 5:
		{
			cal_dur_time(veh, 3, 5, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "Enone", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		}
		break;
	}
	case 4:
	{
		switch (oo) {
		case 1:
		{
			if (sm_veh)  NW_s++;
			else  NW_l++;
			cal_dur_time(veh, 4, 1, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "NW", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 2:
		{
			if (sm_veh)  NS_s++;
			else  NS_l++;
			cal_dur_time(veh, 4, 2, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "NS", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 3:
		{
			if (sm_veh)  NE_s++;
			else  NE_l++;
			cal_dur_time(veh, 4, 3, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "NE", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 4:
		{
			float dur_time = veh.size() / 25.0;    //25帧/s，dur_time单位为s
			veh_set[veh_count].init(ff, "NN", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 5:
		{
			cal_dur_time(veh, 4, 5, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "Nnone", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		}
		break;
	}
	case 5:
		switch (oo) {
		case 1:
		{
			cal_dur_time(veh, 5, 1, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "noneW", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 2:
		{
			cal_dur_time(veh, 5, 2, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "noneS", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 3:
		{
			cal_dur_time(veh, 5, 3, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "noneE", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		case 4:
		{
			cal_dur_time(veh, 5, 4, in_time, out_time);
			float dur_time = (out_time - in_time) / 25.0;
			veh_set[veh_count].init(ff, "noneN", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break; 
		}
		case 5:
		{
			float dur_time = veh.size() / 25.0;    //25帧/s，dur_time单位为s
			veh_set[veh_count].init(ff, "none", veh_size, sm_veh, dur_time, ave_spd);
			veh_count++;
			break;
		}
		}
	}
}

bool lane_W(Point p)
{
	if (W_lane[1].a*p.x + p.y + W_lane[1].c > 0 and
		W_lane[4].a*p.x + p.y + W_lane[4].c < 0 and
		W_lane[0].a*p.x + p.y + W_lane[0].c > 0)
		return true;
	else
		return false;
}
bool lane_S(Point p)
{
	if (S_lane[1].a*p.x + p.y + S_lane[1].c < 0 and
		S_lane[4].a*p.x + p.y + S_lane[4].c > 0 and
		S_lane[0].a*p.x + p.y + S_lane[0].c > 0)
		return true;
	else
		return false;
}
bool lane_E(Point p)
{
	if (E_lane[1].a*p.x + p.y + E_lane[1].c < 0 and
		E_lane[4].a*p.x + p.y + E_lane[4].c > 0 and
		E_lane[0].a*p.x + p.y + E_lane[0].c < 0)
		return true;
	else
		return false;
}
bool lane_N(Point p)
{
	if (N_lane[1].a*p.x + p.y + N_lane[1].c > 0 and
		N_lane[5].a*p.x + p.y + N_lane[5].c < 0 and
		N_lane[0].a*p.x + p.y + N_lane[0].c < 0)
		return true;
	else
		return false;
}

int judge_area2(Point p)     //判断点在哪个进出口道
{
	//注意：交叉口所在位置不同影响结果
	//area_line:W,S,E,N
	if (lane_W(p))  	return 1;    //W口
	else if (lane_S(p))     return 2;    //S口
	else if (lane_E(p))     return 3;    //E口
	else if (lane_N(p))  	return 4;    //N口
	else return 5;    //无
}

int judge_lane(Point p)
{
	switch (judge_area2(p))
	{
	case 1:     //西进口
	{
		if ((W_lane[2].a*p.x + p.y + W_lane[2].c) < 0)
			return 1;
		else if ((W_lane[3].a*p.x + p.y + W_lane[3].c) < 0)
			return 2;
		else
			return 3;
		break;
	}
	case 2:
	{
		if ((S_lane[2].a*p.x + p.y + S_lane[2].c) < 0)
			return 4;
		else if ((S_lane[3].a*p.x + p.y + S_lane[3].c) < 0)
			return 5;
		else
			return 6;
		break;
	}
	case 3:
	{
		if ((E_lane[2].a*p.x + p.y + E_lane[2].c) < 0)
			return 7;
		else if ((E_lane[3].a*p.x + p.y + E_lane[3].c) < 0)
			return 8;
		else
			return 9;
		break;
	}
	case 4:
	{
		if ((N_lane[2].a*p.x + p.y + N_lane[2].c) < 0)
			return 10;
		else if ((N_lane[3].a*p.x + p.y + N_lane[3].c) < 0)
			return 11;
		else if ((N_lane[4].a*p.x + p.y + N_lane[4].c) < 0)
			return 12;
		else
			return 13;
		break;
	}
	}
}

void record_headway(vector <vector<int>> &arrive_time, vector <vector<int>> &head_veh, vector<Point> P_set, int n)    //某车的所有轨迹点,车号
{
	Point P0 = P_set[0];
	int area = judge_area2(P0);
	switch(area)
	{
	case 1:     //W进口
	{
		Point p;
		if (W_lane[2].a*P0.x + P0.y + W_lane[2].c < 0)     //W1,<lW2
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((W_lane[0].a*p.x + p.y + W_lane[0].c) < 0)    //是否越过停车线front
				{
					arrive_time[0].push_back(Track_app_frame[n][i]);
					head_veh[0].push_back(n);
					break;
				}
			}
		}
		else if (W_lane[3].a*P0.x + P0.y + W_lane[3].c < 0)
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((W_lane[0].a*p.x + p.y + W_lane[0].c) < 0)    //是否越过停车线front
				{
					arrive_time[1].push_back(Track_app_frame[n][i]);
					head_veh[1].push_back(n);
					break;
				}
			}
		}
		else
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((W_lane[0].a*p.x + p.y + W_lane[0].c) < 0)    //是否越过停车线front
				{
					arrive_time[2].push_back(Track_app_frame[n][i]);
					head_veh[2].push_back(n);
					break;
				}
			}
		}
		break;
	}
	case 2:     //S进口
	{
		Point p;
		if (S_lane[2].a*P0.x + P0.y + S_lane[2].c > 0)     
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((S_lane[0].a*p.x + p.y + S_lane[0].c) < 0)    //是否越过停车线front
				{
					arrive_time[3].push_back(Track_app_frame[n][i]);
					head_veh[3].push_back(n);
					break;
				}
			}
		}
		else if (S_lane[3].a*P0.x + P0.y + S_lane[3].c > 0)
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((S_lane[0].a*p.x + p.y + S_lane[0].c) < 0)    //是否越过停车线front
				{
					arrive_time[4].push_back(Track_app_frame[n][i]);
					head_veh[4].push_back(n);
					break;
				}
			}
		}
		else
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((S_lane[0].a*p.x + p.y + S_lane[0].c) < 0)    //是否越过停车线front
				{
					arrive_time[5].push_back(Track_app_frame[n][i]);
					head_veh[5].push_back(n);
					break;
				}
			}
		}
		break;
	}
	case 3:     //E进口
	{
		Point p;
		if (E_lane[2].a*P0.x + P0.y + E_lane[2].c > 0)     //W1,<lW2
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((E_lane[0].a*p.x + p.y + E_lane[0].c) > 0)    //是否越过停车线front
				{
					arrive_time[6].push_back(Track_app_frame[n][i]);
					head_veh[6].push_back(n);
					break;
				}
			}
		}
		else if (E_lane[3].a*P0.x + P0.y + E_lane[3].c > 0)
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((E_lane[0].a*p.x + p.y + E_lane[0].c) > 0)    //是否越过停车线front
				{
					arrive_time[7].push_back(Track_app_frame[n][i]);
					head_veh[7].push_back(n);
					break;
				}
			}
		}
		else
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((E_lane[0].a*p.x + p.y + E_lane[0].c) > 0)    //是否越过停车线front
				{
					arrive_time[8].push_back(Track_app_frame[n][i]);
					head_veh[8].push_back(n);
					break;
				}
			}
		}
		break;
	}
	case 4:    //N进口
	{
		Point p;
		if (N_lane[2].a*P0.x + P0.y + N_lane[2].c < 0)     //N1,<lN2
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((N_lane[0].a*p.x + p.y + N_lane[0].c) > 0)    //是否越过停车线front
				{
					arrive_time[9].push_back(Track_app_frame[n][i]);
					head_veh[9].push_back(n);
					break;
				}
			}
		}
		else if (N_lane[3].a*P0.x + P0.y + N_lane[3].c < 0)
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((N_lane[0].a*p.x + p.y + N_lane[0].c) > 0)    //是否越过停车线front
				{
					arrive_time[10].push_back(Track_app_frame[n][i]);
					head_veh[10].push_back(n);
					break;
				}
			}
		}
		else if (N_lane[4].a*P0.x + P0.y + N_lane[4].c < 0)
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((N_lane[0].a*p.x + p.y + N_lane[0].c) > 0)    //是否越过停车线front
				{
					arrive_time[11].push_back(Track_app_frame[n][i]);
					head_veh[11].push_back(n);
					break;
				}
			}
		}
		else
		{
			for (int i = 0; i < P_set.size(); i++)
			{
				p = P_set[i];
				if ((N_lane[0].a*p.x + p.y + N_lane[0].c) > 0)    //是否越过停车线front
				{
					arrive_time[12].push_back(Track_app_frame[n][i]);
					head_veh[12].push_back(n);
					break;
				}
			}
		}
		break;
	}
	}
}

int main()
{
	MultiTracker trackers;    //定义跟踪器
	VideoCapture cap("D:/test/C_1024_540.mp4");           //C放大
	if (!cap.isOpened())
	{
		cout << "Couldn't open the capture!" << endl;
		return -1;
	}

	//************定义开始*************	
	//画面
	Mat frame;			//当前帧
	Mat foreground;		//前景
	Mat middle;     //显示中间过程
	Mat bw;				//中间二值变量
	Mat se1, se2, se3;				//形态学结构元素
	
	//轮廓相关
	double min_veh_sz = 300.0;
	
	//形态学相关
	se1 = getStructuringElement(MORPH_RECT, Size(1, 1));       //可改 膨胀、开闭颗粒度
	se2 = getStructuringElement(MORPH_RECT, Size(2, 2));
	se3 = getStructuringElement(MORPH_RECT, Size(3, 3));

	//跟踪相关
	Mat result;		//跟踪结果
	int AAA = 0;    //判断帧数
	int staleNum = 10;   //最大静止帧数
	Point cenpointn;   //临时变量
	int bd_LU_x = 70; int bd_LU_y = 70;     //bounding左上角坐标
	int bd_x_length = 1024 - 2 * bd_LU_x; int bd_y_length = 540 - 2 * bd_LU_y;       //bounding边长
	Rect bounding = Rect(bd_LU_x, bd_LU_y, bd_x_length, bd_y_length);     //判断车辆是否进入的边界
	vector<Ptr<Tracker>> TrackerVec;    //跟踪器
	vector<int> nonexist;    //记录不需要跟踪的目标

	//************定义完成*************
	
	for (int i = 0; i < 150; i++)      //跳过帧数
	{
		cap >> frame;
	}

	//用户定义进口道区域部分
	cap >> img;
	showImg = img.clone();
	imshow("img", showImg);    //窗口名和setMouseCallback匹配
	waitKey(30);

	cout << "请依次输入交叉口区域四条分隔线，按照西、南、东、北顺序..." << "\n";
	for (int i = 0; i < 4; i++)
	{
		get_roi(i);
		area_line[i] = cal_line(P1, P2);
	}
	cout << "输入完成！" << "\n"<< "已选区域：\n";

	cout << "四条边界直线：" << '\n';
	cout << "西直线：" << area_line[0].a << "x+" << "y+" << area_line[0].c << "=0" << '\n';
	cout << "南直线：" << area_line[1].a << "x+" <<  "y+" << area_line[1].c << "=0" << '\n';
	cout << "东直线：" << area_line[2].a << "x+" << "y+" << area_line[2].c << "=0" << '\n';
	cout << "北直线：" << area_line[3].a << "x+" <<  "y+" << area_line[3].c << "=0" << '\n';

	//用户定义车道区域部分
	cout << "请依次输入交叉口车道分界线，按照西、南、东、北顺序..." << "\n";
	cout << "请依次输入西进口道车道分界线，按照前，1，2，3顺序..." << "\n";
	for (int i = 0; i < 5; i++)
	{
		get_roi(0);
		W_lane[i] = cal_line(P1, P2);
	}
	cout << "请依次输入南进口道车道分界线，按照前，1，2，3顺序..." << "\n";
	for (int i = 0; i < 5; i++)
	{
		get_roi(1);
		S_lane[i] = cal_line(P1, P2);
	}
	cout << "请依次输入东进口道车道分界线，按照前，1，2，3顺序..." << "\n";
	for (int i = 0; i < 5; i++)
	{
		get_roi(2);
		E_lane[i] = cal_line(P1, P2);
	}
	cout << "请依次输入北进口道车道分界线，按照前，1，2，3，4顺序..." << "\n";
	for (int i = 0; i < 6; i++)
	{
		get_roi(3);
		N_lane[i] = cal_line(P1, P2);
	}
	cout << "输入完成！" << "\n" << "已选区域：\n";
	
	//计算比例尺部分
	cout << "按任意键开始计算比例尺..." << "\n";
	waitKey(0);
	get_scale();
	cout << "输入完成！" << "\n";
	
	cout << "按任意键开始训练背景..." << "\n";
	waitKey(0);
	destroyWindow("img");
	destroyWindow("选择区域");

	//开始训练背景及目标识别部分

	//用混合高斯模型训练背景图像
	Ptr<BackgroundSubtractorMOG2> bgsubtractor = createBackgroundSubtractorMOG2();
	// 背景模型影响帧数 默认为500
	bgsubtractor->setHistory(200);
	// 模型匹配阈值
	//bgsubtractor->setVarThreshold(25);
	//背景阈值设定 backgroundRatio*history
	//bgsubtractor->setBackgroundRatio(2);
	////设置阈值的降低的复杂性
	//bgsubtractor->setComplexityReductionThreshold(0.02);
	////高斯混合模型组件数量,默认5
	//bgsubtractor->setNMixtures(5);
	////设置每个高斯组件的初始方差
	//bgsubtractor->setVarInit(0.5);
	////新模型匹配阈值
	//bgsubtractor->setVarThresholdGen(7);

	for (int i = 0; i <= 30; ++i)
	{
		cout << "正在训练背景:" << i << endl;
		cap >> frame;
		if (frame.empty() == true)
		{
			cout << "视频帧太少，无法训练背景" << endl;
			getchar();
			return 0;
		}
		bgsubtractor->apply(frame, foreground, 0.01);
	}

	//初始化搜索框
	cap >> frame;     //仅输入一帧

	//检测目标(其实是边训练边检测)，记录到foreground里
	bgsubtractor->apply(frame, foreground, 0.01);     //背景减除
	threshold(foreground, foreground, 127, 255, CV_THRESH_TOZERO);     //去除阴影
	morph_ope(se1, se2, se3, foreground);        //形态学操作
	//检索前景中各个连通分量的轮廓
	foreground.copyTo(bw);    //foreground拷贝给bw
	findContours(bw, contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);      //在bw中找轮廓，储存到contours
	
	vector<vector<Point>>::iterator it_border1;
	for (it_border1 = contours.begin(); it_border1 != contours.end();)      //删去超出边界的
	{
		Point rt_pt = getCenterPoint(boundingRect(*it_border1));
		if (!judge_rect(rt_pt, bounding))
			it_border1 = contours.erase(it_border1);
		else
			++it_border1;
	}

	//对连通分量进行排序
	std::sort(contours.begin(), contours.end(), biggerSort);

	int rt_size = 0;     //仅局部使用，为rt中非0Rect的数量
	for (int k = 0; k < contours.size(); ++k)  //对每一个轮廓,得到rt[k],即初始搜索框
	{
		//第k个连通分量的外接矩形框
		if (contourArea(contours[k]) < max(contourArea(contours[0]) / 5 , min_veh_sz))   //不考虑过小框，可改
			break;      //用break是因为contours已经按照从小到大排列了
		if (contourArea(contours[k]) > 60 * 60)          //不考虑过大框，可接受车辆大小60*60，可改
			break;
		rt.push_back(boundingRect(contours[k]));   //rt是contours的边界框，已删除过大/小框
		//筛选完毕
	}

	for (size_t i = 0; i < rt.size(); i++)    //对每个搜索窗口
	{
		ROI.push_back(rt[i]);      //加入ROI
		// create the tracker
		TrackerVec.push_back(TrackerKCF::create());
		// initialize the tracker
		TrackerVec[i]->init(frame, rt[i]);     //TrackerVec[i]是tracker
		Record_ROI[AAA].push_back(rt[i]);
		Track[i].push_back(rt[i]);      //i为初始车号
		Track_app_frame[i].push_back(AAA);
		//name.push_back(i);        //更新车号
	}

	AAA++;

	// 开始跟踪部分
	printf("开始跟踪，按退出键退出\n");

	while (1)       //对每一帧循环
	{
		cap >> frame;
		if (frame.rows == 0 || frame.cols == 0)
			break;
		frame.copyTo(result);
		//frame.copyTo(middle);

		//产生新的（与已有的不重叠的）ROI
		vector<Rect> obj = generate(bgsubtractor, frame,middle, foreground, se1, se2, se3, bw, AAA, min_veh_sz, veh_num, bounding, TrackerVec);

		//加入新对象
		for (int x = 0; x < obj.size(); x++)       
		{
			if (lostP.size() == 0)    //若无丢车
			{
				cout << "无丢车get a new object" << ROI.size() << endl;

				ROI.push_back(obj[x]);       //加到搜索框
				// create the tracker
				TrackerVec.push_back(TrackerKCF::create());
				int sz = TrackerVec.size();
				// initialize the tracker
				TrackerVec[sz - 1]->init(frame, obj[x]);     //TrackerVec[i]是tracker

				//rectangle(frame, obj[x], Scalar(24, 25, 226), 2);    //红色标出新加object
				break;
			}

			bool islost = false;
			for (int y = 0; y < lostP.size(); y++)      //防止跟丢
			{
				Point P1 = lostP[y];     //跟丢的车
				Point P2 = getCenterPoint(obj[x]);    //当前车
				if ((P1.x - 15 < P2.x) && (P1.x + 15 > P2.x) &&
					(P1.y - 15 < P2.y) && (P1.y + 15 > P2.y))
					//如果找到了这个对应的lostP[y]
				{
					cout << "抓回跟丢车：" << lostN[y] << '\n';
					ROI[lostN[y]] = obj[x];       //加到原位置
					//Record_ROI[AAA].push_back(obj[x]);
					//[lostN[y]].push_back(obj[x]);
					//Track_app_frame[lostN[y]].push_back(AAA);     重复了
					//重新初始化原来的跟踪器
					TrackerVec[lostN[y]]->init(frame, obj[x]);     //TrackerVec[i]是tracker

					//删掉丢车记录
					for (int i = 0; i < nonexist.size(); i++)
					{
						if (nonexist[i] == lostN[y])
						{
							vector<int>::iterator ne_pt = nonexist.begin() + i;
							nonexist.erase(ne_pt);
						}
					}
					vector<Point>::iterator it_dele_veh = lostP.begin() + y;
					lostP.erase(it_dele_veh);
					vector<int>::iterator it_dele_num = lostN.begin() + y;
					lostN.erase(it_dele_num);
					vector<int>::iterator it_dele_time = losttime.begin() + y;
					losttime.erase(it_dele_time);
					
					//rectangle(frame, obj[x], Scalar(197, 95, 134), 2);    //紫色标出新加object
					islost = true;
					break;     //找到就退出
				}			
			}	
			if (!islost)
			{
				cout << "判断不是跟丢车的新车：" << ROI.size() << endl;
				ROI.push_back(obj[x]);       //加到搜索框
				// create the tracker
				TrackerVec.push_back(TrackerKCF::create());
				int sz = TrackerVec.size();
				// initialize the tracker
				TrackerVec[sz - 1]->init(frame, obj[x]);     //TrackerVec[i]是tracker

				//rectangle(frame, obj[x], Scalar(24, 25, 226), 2);    //红色标出新加object
			}
			//name.push_back(name[name.size() - 1] + 1);
			//cout << "get a new object" << endl;
			//rectangle(frame, obj[x], Scalar(24, 25, 226), 2);    //红色标出新加object
		}
		
		//对所有搜索目标,进行更新
		for (int m = 0; m < TrackerVec.size(); m++)  
		{
			if (m > ROI.size()) break;
			bool isfound;

			// update the tracking result
			if (find(nonexist.begin(), nonexist.end(), m) == nonexist.end())
				isfound = TrackerVec[m]->update(frame, ROI[m]);      //更新ROI窗口
			else
				continue;

			if (Track[m].size() > 1
				and ROI[m].x == Track[m][Track[m].size() - 2].x and ROI[m].y == Track[m][Track[m].size() - 2].y
				and ROI[m].x == Track[m][Track[m].size() - 1].x and ROI[m].y == Track[m][Track[m].size() - 1].y)
				isfound = false;    //三帧位置未变

			int cri = 0;
			//删去所有新的搜索框里重叠的其中一个（除了(0,0,0,0)以外！）
			//for (int n = 0; n < ROI.size(); n++)      
			//{
			//	if (n >= ROI.size() or m >= ROI.size() or m == n) break;
			//	Rect M = Rect(ROI[m]);
			//	Rect N = Rect(ROI[n]);
			//	if (isOverlap(M, N) == true and overlapRatio(M, N) > 0.6)         //可调
			//		if (M.area() < N.area())   //删去M
			//		{
			//			vector<Rect2d>::iterator iter4 = ROI.begin() + m;
			//			ROI.erase(iter4);
			//			vector<Ptr<Tracker>>::iterator iter6 = TrackerVec.begin() + m;
			//			TrackerVec.erase(iter6);
			//			for (int a = 0; a < AAA; a++)
			//			{
			//				if (Record_ROI[a].size() > m)
			//				{
			//					vector<Rect>::iterator iter2 = Record_ROI[a].begin() + m;    //清0每一帧的m车，erase是删掉条目
			//					Record_ROI[a].erase(iter2);
			//				}
			//				else
			//					continue;
			//			}
			//		}
			//		else              //删去N
			//		{
			//			vector<Rect2d>::iterator iter5 = ROI.begin() + n;
			//			ROI.erase(iter5);
			//			vector<Ptr<Tracker>>::iterator iter7 = TrackerVec.begin() + n;
			//			TrackerVec.erase(iter7);
			//			for (int a = 0; a < AAA; a++)
			//			{
			//				if (Record_ROI[a].size() > m)
			//				{
			//					vector<Rect>::iterator iter2 = Record_ROI[a].begin() + m;    //清0每一帧的m车，erase是删掉条目
			//					Record_ROI[a].erase(iter2);
			//				}
			//				else
			//					continue;
			//			}
			//		}
			//}

			if (!isfound)      //若不能跟踪
			{
				lostP.push_back(getCenterPoint(ROI[m]));
				lostN.push_back(m);				
				losttime.push_back(0);
				cout << "跟丢车：" << m << '\n';
				cout << "现跟丢共" << lostP.size() << "辆车" << '\n';
				rectangle(frame, ROI[m], Scalar(255, 0, 0), 2);   

				//if (lostP.size() > 2)    //删去重叠的跟踪车
				//{
				//	for (int i = 0; i < lostP.size(); i++)
				//	{
				//		for (int z = i + 1; z < lostP.size(); z++)
				//			if (lostP[i].x - 10 < lostP[z].x and lostP[i].x + 10 > lostP[z].x and
				//				lostP[i].y - 10 < lostP[z].y and lostP[i].y + 10 > lostP[z].y)
				//				//如果两个重叠，则删去前一个
				//			{
				//				vector<Point>::iterator it_veh = lostP.begin() + i;
				//				lostP.erase(it_veh);
				//				vector<int>::iterator it_num = lostN.begin() + i;
				//				lostN.erase(it_num);
				//				vector<int>::iterator it_time = losttime.begin() + i;
				//				losttime.erase(it_time);
				//				cout << "丢失车重叠：" << i << '\n';
				//			}
				//	}
				//}

				cri++;
				nonexist.push_back(m);    //记录不跟踪车辆
				ROI[m] = Rect(0, 0, 0, 0);    //保留车号，但是清0跟踪框
				//清0这一帧的m车
			}
			else      //若能跟踪
			{
				Point cp = getCenterPoint(ROI[m]);
				if (!judge_rect(cp, bounding))     //若超过范围
				{
					nonexist.push_back(m);    //记录不跟踪车辆
					ROI[m] = Rect(0, 0, 0, 0);    //保留车号，但是清0跟踪框
					//清0这一帧的m车				
					cout << "删去超出范围的车:" << m << '\n';
					cri++;
				}
				else
				{
					Track[m].push_back(ROI[m]);          //车号会变
					Track_app_frame[m].push_back(AAA);
					Record_ROI[AAA].push_back(ROI[m]);       //存下能跟踪的车
				}
			}

			// draw the tracked object
			rectangle(frame, ROI[m], Scalar(255, 0, 0), 2);    //若跟丢车，此时ROI已为0
		}
		for (int z = 0; z < losttime.size(); z++)    //给现有losttime各加1
		{
			if (losttime[z] < 10)
				losttime[z]++;
			else
			{   //删掉丢了超过10帧的车
				vector<Point>::iterator it_dele_veh = lostP.begin() + z;
				lostP.erase(it_dele_veh);
				vector<int>::iterator it_dele_num = lostN.begin() + z;
				lostN.erase(it_dele_num);
				vector<int>::iterator it_dele_time = losttime.begin() + z;
				losttime.erase(it_dele_time);
				cout << "删掉丢了超过10帧的车" << z << '\n';
			}
		}

		cout << "ROI:";
		for (int z = 0; z < ROI.size(); z++)
		{
			if (ROI[z] != Rect2d(0, 0, 0, 0))
				cout << z << ",";
		}
		cout << '\n';

		//画跟踪轨迹于frame
		cv::RNG rng(0xFFFFFFFF);
		for (int x = 0; x < Track.size(); x++)   //标注轨迹
		{
			Scalar SC = randomColor(rng);
			int count2 = 0;

			//在每辆车最后一帧标注文字
			string S;
			for (int n = 0; n < Track[x].size(); n++)
			{
				cenpointn = getCenterPoint(Track[x][n]);
				circle(result, cenpointn, 2, SC, 2);     //Scalar(0, 0, 255)
				if (n == Track[x].size() - 1)
				{
					S = to_string(x);
					Point X = Point(cenpointn.x + 10, cenpointn.y + 10);
					putText(result, S, X, FONT_HERSHEY_SIMPLEX, 0.6, SC, 2);
				}
			}
		}
		imshow("跟踪情况", result);
		cv::waitKey(30);

		imshow("tracker", frame);
		cv::waitKey(30);

		//quit on ESC button
		if (cv::waitKey(1) == 27)break;

		AAA++;
		cout << AAA << endl;
		for (int a = 0; a < name.size(); a++)
			cout << name[a] << ",";
		if (AAA == end_AAA) break;

	}///////////帧数循环//////////////

	//各进口道流量计算
	float time = AAA / 90000.0;     //25帧/s，测试总时长(h)
	int not_count = 0;

	int track_act_size = 0;     //计算track内非空车部分size
	for (int xx = 0; xx < Track.size(); xx++)
	{
		if (Track[xx].size() != 0)
			track_act_size++;
		else
			break;
	}
		
	vector<vector<Point>> cenpoint_set(track_act_size);    //矩心矩阵，和track的维数一样，计算track各rect的矩形位置
	vector<vector<float>> cenpoint_img_dis(track_act_size);    //图上距离（相邻两帧间同一车辆）
	vector<vector<float>> cenpoint_act_dis(track_act_size);    //实际距离（相邻两帧间同一车辆）
	vector<vector<float>> cenpoint_spd(track_act_size);     //车辆每帧实际速度
	vector<vector<float>> x_act_spd(track_act_size);    //实际x向速度（相邻两帧间同一车辆）
	vector<vector<float>> y_act_spd(track_act_size);    //实际y向速度（相邻两帧间同一车辆）
	vector<vector<float>> spd_degree(track_act_size);    //实际速度角度（相邻两帧间同一车辆）
	vector<vector<String>> spd_direction(track_act_size);    //实际速度方向（相邻两帧间同一车辆）
	vector<float> cenpoint_ave_spd(track_act_size);     //车辆所有帧均速
	float pi = 3.1415926;

	for (int ff = 0; ff < track_act_size; ff++)    //得到矩心矩阵，注意：帧数<10的车辆即[ff]为空！
	{
		for (int gg = 0; gg < Track[ff].size(); gg++)     //得到有用车辆的矩心矩阵
		{
			cenpoint_set[ff].push_back(getCenterPoint(Track[ff][gg]));
		}

		float tt_spd = 0.0;
		for (int gg = 0; gg < cenpoint_set[ff].size() - 1; gg++)     //得到有用车辆的每帧速度
		{
			Point p1 = cenpoint_set[ff][gg];
			Point p2 = cenpoint_set[ff][gg + 1];
			int xx = p2.x - p1.x;
			int yy = p2.y - p1.y;
			float xx_f = xx;
			float yy_f = yy;
			int xx2 = pow((p2.x - p1.x), 2);
			int yy2 = pow((p2.y - p1.y), 2);
			float distance = sqrt(xx2 + yy2);
			cenpoint_img_dis[ff].push_back(distance);
			cenpoint_act_dis[ff].push_back(distance * scale);

			double tempo = scale * 25 / (Track_app_frame[ff][gg + 1] - Track_app_frame[ff][gg]);
			cenpoint_spd[ff].push_back(distance * tempo);    //实际速度，单位m/s
			x_act_spd[ff].push_back(xx * tempo);      //计算x向实际速度
			y_act_spd[ff].push_back(yy * tempo);      //计算y向实际速度
			spd_degree[ff].push_back(atan(abs(yy_f / xx_f)) / pi * 180);    //计算方向度数
			if (xx==0 && yy==0)
				spd_direction[ff].push_back("none");
			else if (xx >= 0 && yy >= 0)
				spd_direction[ff].push_back("ES");
			else if (xx >= 0 && yy <= 0)
				spd_direction[ff].push_back("EN");
			else if (xx <= 0 && yy <= 0)
				spd_direction[ff].push_back("WN");
			else
				spd_direction[ff].push_back("WS");
			tt_spd += distance * scale * 25;
		}
		cenpoint_ave_spd[ff] = tt_spd / cenpoint_set[ff].size();    //记录均速
	}

	for (int ff = 0; ff < track_act_size; ff++)    //得到矩心矩阵，注意：帧数<30的车辆即[ff]为空！
	{
		if (Track[ff].size() > 30)      //若其存在帧数>30
		{
			Point cenpoint_ini;     //初始位置
			Point cenpoint_last;    //末位置
			cenpoint_ini = getCenterPoint(Track[ff][0]);
			cenpoint_last = getCenterPoint(Track[ff][Track[ff].size() - 1]);

			float veh_size_sum = 0.0;
			float veh_count_sum = 0.0;
			for (int gg = 0; gg < Track[ff].size(); gg++) 
			{
				if (Track[ff][gg].area() != 0)
				{
					veh_size_sum += Track[ff][gg].area();
					veh_count_sum += 1;
				}
			}
			float veh_size = veh_size_sum / veh_count_sum;    //计算平均车大小
			bool sm_veh = true;

			if (veh_size > 1900)     //判断小/大车
				sm_veh = false;

			veh_result(cenpoint_ini, cenpoint_last, cenpoint_set[ff], ff, veh_size, sm_veh, cenpoint_ave_spd[ff]);
		}
		else
		{
			not_count++;
		}
	}

	cout << "\n各个进口道车辆数：\n" << "（小车/大车）\n"
		<< "北进口：\n左：" << NE_s << " / " << NE_l << "\t直：" << NS_s << " / " << NS_l << "\t右：" << NW_s << " / " << NW_l
		<< "\n西进口：\n左：" << WN_s << " / " << WN_l << "\t直：" << WE_s << " / " << WE_l << "\t右：" << WS_s << " / " << WS_l
		<< "\n南进口：\n左：" << SW_s << " / " << SW_l << "\t直：" << SN_s << " / " << SN_l << "\t右：" << SE_s << " / " << SE_l
		<< "\n东进口：\n左：" << ES_s << " / " << ES_l << "\t直：" << EW_s << " / " << EW_l << "\t右：" << EN_s << " / " << EN_l << "\n";
	cout << "未计入轨迹数：" << not_count << "\n";

	cout << "\n各个进口道流量：\n" << "（小车/大车）\n"
		<< "北进口：\n左：" << NE_s / time << " / " << NE_l / time << "\t直：" << NS_s / time << " / " << NS_l / time 
		<< "\t右：" 	<< NW_s / time << " / " << NW_l / time << "\n西进口：\n左：" << WN_s / time << " / " << WN_l / time 
		<< "\t直：" << WE_s / time << " / " << WE_l / time << "\t右：" << WS_s / time << " / " << WS_l / time
		<< "\n南进口：\n左：" << SW_s / time << " / " << SW_l / time << "\t直：" << SN_s / time << " / " << SN_l / time
		<< "\t右：" << SE_s / time << " / " << SE_l / time << "\n东进口：\n左：" << ES_s / time << " / " << ES_l / time
		<< "\t直：" << EW_s / time << " / " << EW_l / time << "\t右：" << EN_s / time << " / " << EN_l / time << "\n";

	ofstream outFile1;
	outFile1.open("result_track.csv", ios::out);     // 打开模式可省略
	outFile1 << "车辆" << ',' << "总帧数" << ',' << "出现帧数" << ',' << "像素x坐标" << ',' << "像素y坐标" << ',' << "跟踪框宽" << ',' << "跟踪框高" << ','<<"图上距离"
		<< ',' << "实际距离" << ',' << "实际速度" << ',' << "x向实际速度" << ',' << "y向实际速度" << ',' << "速度角度" << ',' << "速度方向" << endl;

	for (int ff = 0; ff < cenpoint_set.size(); ff++)    //输出表格,车辆矩心坐标及帧数
	{
		if (Track[ff].size() > 30)
		{
			for (int gg = 0; gg < cenpoint_set[ff].size() - 1; gg++)
			{
				outFile1 << ff << ',' << Track_app_frame[ff][gg] << ',' << gg << ',' << cenpoint_set[ff][gg].x << ',' << cenpoint_set[ff][gg].y
					<< ',' << Track[ff][gg].width << ',' << Track[ff][gg].height << ',' << cenpoint_img_dis[ff][gg] << ',' << cenpoint_act_dis[ff][gg] 
					<< ',' << cenpoint_spd[ff][gg] << ',' << x_act_spd[ff][gg] << ',' << y_act_spd[ff][gg] << ',' << spd_degree[ff][gg] 
					<< ',' << spd_direction[ff][gg] << endl;
			}
		}	
	}
	cout << "已完成储存result_track.csv！\n";
	outFile1.close();

	ofstream outFile2;
	outFile2.open("result_veh.csv", ios::out);     // 打开模式可省略
	outFile2 << "车辆编号" << ',' << "转向" << ',' << "跟踪框图上面积（平方像素）" << ',' << "是否为小车" << ',' << "通过交叉口时间（s）" << ',' 
		<< "车辆均速（m/s）" << endl;
	for (int aa = 0; aa < veh_count; aa++)    //输出表格
	{
		outFile2 << veh_set[aa].name << ',' << veh_set[aa].direct << ',' << veh_set[aa].veh_size << ',' << veh_set[aa].sm_veh 
			<< ',' << veh_set[aa].dur_time << ',' << veh_set[aa].ave_spd  << endl;
	}
	cout << "已完成储存result_veh.csv！\n";
	outFile2.close();

	//计算车头时距

	vector<vector<int>> arrive_time(13); 
	vector<vector<int>> head_veh(13);
	vector<vector<float>> headway(13);

	for (int a = 0; a < cenpoint_set.size();a++)      //对所有轨迹，不加筛选
		record_headway(arrive_time, head_veh, cenpoint_set[a],a);

	for (int b = 0; b < arrive_time.size(); b++)
		sort(arrive_time[b].begin(), arrive_time[b].end());
	
	for (int c = 0; c < arrive_time.size(); c++)
	{
		if (arrive_time[c].size() > 1)
		{
			for (int d = 0; d < arrive_time[c].size() - 1; d++)
				headway[c].push_back((arrive_time[c][d + 1] - arrive_time[c][d]) / 25.0);    //除以帧数就是时间
		}
	}
		
	ofstream outFile3;
	outFile3.open("result_headway.csv", ios::out);     // 打开模式可省略
	vector<String> lane_name = { "西进口车道1" ,"西进口车道2","西进口车道3", "南进口车道1", "南进口车道2", "南进口车道3",
		"东进口车道1", "东进口车道2","东进口车道3", "北进口车道1" ,"北进口车道2","北进口车道3","北进口车道4"};
	for (int i = 0; i < 13; i++)    //输出表格
	{
		outFile3 << lane_name[i] << ',';
		for (int j = 0; j < headway[i].size(); j++)
		{
			if (j == headway[i].size() - 1)
				outFile3 << headway[i][j] << endl;
			else
				outFile3 << headway[i][j] << ',';		
		}
		if (headway[i].size() == 0)
			outFile3 << endl;
	}
	cout << "已完成储存result_headway.csv！\n";
	outFile3.close();

	getchar();
}/////////////////int main////////////