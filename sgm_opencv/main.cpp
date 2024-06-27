/******************************/
/*        ����ƥ��Ͳ��        */
/******************************/
#define _CRT_SECURE_NO_WARNINGS

#include <opencv2/opencv.hpp>  
#include <iostream>  
#include <stdio.h>

using namespace std;
using namespace cv;

const int imageWidth = 1920;                             //����ͷ�ķֱ���  
const int imageHeight = 1024;
Size imageSize = Size(imageWidth, imageHeight);

Mat rgbImageL, grayImageL;
Mat rgbImageR, grayImageR;
Mat rectifyImageL, rectifyImageR;

Rect validROIL;//ͼ��У��֮�󣬻��ͼ����вü��������validROI����ָ�ü�֮�������  
Rect validROIR;

Mat mapLx, mapLy, mapRx, mapRy;     //ӳ���  
Mat Rl, Rr, Pl, Pr, Q;              //У����ת����R��ͶӰ����P ��ͶӰ����Q
Mat xyz;              //��ά����

Point origin;         //��갴�µ���ʼ��
Rect selection;      //�������ѡ��
bool selectObject = false;    //�Ƿ�ѡ�����


Ptr<StereoSGBM> sgbm = StereoSGBM::create(0, 16, 3);

/*
���ȱ궨�õ�����Ĳ���
fx 0 cx
0 fy cy
0 0  1
*/
Mat cameraMatrixL = (Mat_<double>(3, 3) << 4334.09568, 0, 959.50000,
	0, 4334.09568, 511.50000,
	0, 0, 1.0);
Mat distCoeffL = (Mat_<double>(5, 1) << 0.0, 0.0, 0.0, 0.0, 0.0);

Mat cameraMatrixR = (Mat_<double>(3, 3) << 4489.55770, 0, 801.86552,
	0, 4507.13412, 530.72579,
	0, 0, 1.0);
Mat distCoeffR = (Mat_<double>(5, 1) << 0.0, 0.0, 0.0, 0.0, 0.0);

Mat T = (Mat_<double>(3, 1) << -518.97666, 01.20629, 9.14632);//Tƽ������
Mat rec = (Mat_<double>(3, 1) << 0.04345, -0.05236, -0.01810);//rec��ת����
Mat R;//R ��ת����


static void saveXYZ(const char* filename, const Mat& mat)
{
	const double max_z = 16.0e4;
	FILE* fp = fopen(filename, "wt");

	//FILE* fp = NULL;
	//fopen_s(fp, filename, "wt");

	printf("%d %d \n", mat.rows, mat.cols);
	for (int y = 0; y < mat.rows; y++)
	{
		for (int x = 0; x < mat.cols; x++)
		{
			Vec3f point = mat.at<Vec3f>(y, x);
			if (fabs(point[2] - max_z) < FLT_EPSILON || fabs(point[2]) > max_z) continue;
			fprintf(fp, "%f %f %f\n", point[0], point[1], point[2]);

		}
	}
	fclose(fp);
}

/*�����ͼ��ɫ*/
void GenerateFalseMap(cv::Mat& src, cv::Mat& disp)
{
	// color map  
	float max_val = 255.0f;
	float map[8][4] = { { 0,0,0,114 },{ 0,0,1,185 },{ 1,0,0,114 },{ 1,0,1,174 },
	{ 0,1,0,114 },{ 0,1,1,185 },{ 1,1,0,114 },{ 1,1,1,0 } };
	float sum = 0;
	for (int i = 0; i < 8; i++)
		sum += map[i][3];

	float weights[8]; // relative   weights  
	float cumsum[8];  // cumulative weights  
	cumsum[0] = 0;
	for (int i = 0; i < 7; i++) {
		weights[i] = sum / map[i][3];
		cumsum[i + 1] = cumsum[i] + map[i][3] / sum;
	}

	int height_ = src.rows;
	int width_ = src.cols;
	// for all pixels do  
	for (int v = 0; v < height_; v++) {
		for (int u = 0; u < width_; u++) {

			// get normalized value  
			float val = std::min(std::max(src.data[v * width_ + u] / max_val, 0.0f), 1.0f);

			// find bin  
			int i;
			for (i = 0; i < 7; i++)
				if (val < cumsum[i + 1])
					break;

			// compute red/green/blue values  
			float   w = 1.0 - (val - cumsum[i]) * weights[i];
			uchar r = (uchar)((w * map[i][0] + (1.0 - w) * map[i + 1][0]) * 255.0);
			uchar g = (uchar)((w * map[i][1] + (1.0 - w) * map[i + 1][1]) * 255.0);
			uchar b = (uchar)((w * map[i][2] + (1.0 - w) * map[i + 1][2]) * 255.0);
			//rgb�ڴ��������  
			disp.data[v * width_ * 3 + 3 * u + 0] = b;
			disp.data[v * width_ * 3 + 3 * u + 1] = g;
			disp.data[v * width_ * 3 + 3 * u + 2] = r;
		}
	}
}

/*****����ƥ��*****/
void stereo_match(int, void*)
{
	sgbm->setPreFilterCap(63);
	int sgbmWinSize = 5;//����ʵ������Լ��趨
	int NumDisparities = 416;//����ʵ������Լ��趨
	int UniquenessRatio = 6;//����ʵ������Լ��趨
	sgbm->setBlockSize(sgbmWinSize);
	int cn = rectifyImageL.channels();

	sgbm->setP1(8 * cn * sgbmWinSize * sgbmWinSize);
	sgbm->setP2(32 * cn * sgbmWinSize * sgbmWinSize);
	sgbm->setMinDisparity(0);
	sgbm->setNumDisparities(NumDisparities);
	sgbm->setUniquenessRatio(UniquenessRatio);
	sgbm->setSpeckleWindowSize(100);
	sgbm->setSpeckleRange(10);
	sgbm->setDisp12MaxDiff(1);
	sgbm->setMode(StereoSGBM::MODE_SGBM);
	Mat disp, dispf, disp8;
	sgbm->compute(rectifyImageL, rectifyImageR, disp);
	//ȥ�ڱ�
	Mat img1p, img2p;
	copyMakeBorder(rectifyImageL, img1p, 0, 0, NumDisparities, 0, BORDER_REPLICATE);
	copyMakeBorder(rectifyImageR, img2p, 0, 0, NumDisparities, 0, BORDER_REPLICATE);
	dispf = disp.colRange(NumDisparities, img2p.cols - NumDisparities);

	dispf.convertTo(disp8, CV_8U, 255 / (NumDisparities * 16.));
	reprojectImageTo3D(dispf, xyz, Q, true); //��ʵ�������ʱ��ReprojectTo3D������X / W, Y / W, Z / W��Ҫ����16(Ҳ����W����16)�����ܵõ���ȷ����ά������Ϣ��
	xyz = xyz * 16;
	imshow("disparity", disp8);
	Mat color(dispf.size(), CV_8UC3);
	GenerateFalseMap(disp8, color);//ת�ɲ�ͼ
	imshow("disparity", color);
	saveXYZ("xyz.xls", xyz);
	waitKey(0);
}



/*****�������������ص�*****/
static void onMouse(int event, int x, int y, int, void*)
{
	if (selectObject)
	{
		selection.x = MIN(x, origin.x);
		selection.y = MIN(y, origin.y);
		selection.width = std::abs(x - origin.x);
		selection.height = std::abs(y - origin.y);
	}

	switch (event)
	{
	case EVENT_LBUTTONDOWN:   //�����ť���µ��¼�
		origin = Point(x, y);
		selection = Rect(x, y, 0, 0);
		selectObject = true;
		cout << origin << "in world coordinate is: " << xyz.at<Vec3f>(origin) << endl;
		break;
	case EVENT_LBUTTONUP:    //�����ť�ͷŵ��¼�
		selectObject = false;
		if (selection.width > 0 && selection.height > 0)
			break;
	}
}


/*****������*****/
int main()
{
	/*	����У��	*/
	Rodrigues(rec, R); //Rodrigues�任
	stereoRectify(cameraMatrixL, distCoeffL, cameraMatrixR, distCoeffR, imageSize, R, T, Rl, Rr, Pl, Pr, Q, CALIB_ZERO_DISPARITY,
		0, imageSize, &validROIL, &validROIR);
	initUndistortRectifyMap(cameraMatrixL, distCoeffL, Rl, Pl, imageSize, CV_16SC2, mapLx, mapLy);
	initUndistortRectifyMap(cameraMatrixR, distCoeffR, Rr, Pr, imageSize, CV_16SC2, mapRx, mapRy);

	/*	��ȡͼƬ	*/
	rgbImageL = imread("left_cor.bmp", IMREAD_COLOR);//CV_LOAD_IMAGE_COLOR
	rgbImageR = imread("right_cor.bmp", IMREAD_COLOR);

	
	/*	����remap֮�����������ͼ���Ѿ����沢���ж�׼��	*/
	remap(rgbImageL, rectifyImageL, mapLx, mapLy, INTER_LINEAR);//INTER_LINEAR
	remap(rgbImageR, rectifyImageR, mapRx, mapRy, INTER_LINEAR);

	/*	��У�������ʾ����*/

	//��ʾ��ͬһ��ͼ��
	Mat canvas;
	double sf;
	int w, h;
	sf = 700. / MAX(imageSize.width, imageSize.height);
	w = cvRound(imageSize.width * sf);
	h = cvRound(imageSize.height * sf);
	canvas.create(h, w * 2, CV_8UC3);   //ע��ͨ��

	//��ͼ�񻭵�������
	Mat canvasPart = canvas(Rect(w * 0, 0, w, h));                                //�õ�������һ����  
	resize(rectifyImageL, canvasPart, canvasPart.size(), 0, 0, INTER_AREA);     //��ͼ�����ŵ���canvasPartһ����С  
	Rect vroiL(cvRound(validROIL.x * sf), cvRound(validROIL.y * sf),                //��ñ���ȡ������    
		cvRound(validROIL.width * sf), cvRound(validROIL.height * sf));
	//rectangle(canvasPart, vroiL, Scalar(0, 0, 255), 3, 8);                      //����һ������  
	cout << "Painted ImageL" << endl;

	//��ͼ�񻭵�������
	canvasPart = canvas(Rect(w, 0, w, h));                                      //��û�������һ����  
	resize(rectifyImageR, canvasPart, canvasPart.size(), 0, 0, INTER_LINEAR);
	Rect vroiR(cvRound(validROIR.x * sf), cvRound(validROIR.y * sf),
		cvRound(validROIR.width * sf), cvRound(validROIR.height * sf));
	//rectangle(canvasPart, vroiR, Scalar(0, 0, 255), 3, 8);
	cout << "Painted ImageR" << endl;

	//���϶�Ӧ������
	for (int i = 0; i < canvas.rows; i += 16)
		line(canvas, Point(0, i), Point(canvas.cols, i), Scalar(0, 255, 0), 1, 8);
	imshow("rectified", canvas);
	waitKey(0);

	/*	����ƥ��	*/
	namedWindow("disparity", WINDOW_AUTOSIZE);
	//�����Ӧ����setMouseCallback(��������, ���ص�����, �����ص������Ĳ�����һ��ȡ0)
	setMouseCallback("disparity", onMouse, 0);//disparity
	stereo_match(0, 0);


	return 0;
}