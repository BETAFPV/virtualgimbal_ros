/* 
 * Software License Agreement (BSD 3-Clause License)
 * 
 * Copyright (c) 2020, Yoshiaki Sato
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "SO3Filters.h"



/**
     * @brief ワープした時に欠けがないかチェックします
     * @retval false:欠けあり true:ワープが良好
     **/
bool isGoodWarp(std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> &contour, CameraInformationPtr camera_info)
{
    for (const auto &p : contour)
    {
        if ((0. < p[0]) 
        && ( (camera_info->width_ - 1.) > p[0])
        && (0. < p[1])
        && ((camera_info->height_ - 1.) > p[1]))
        {
            return false;
        }
    }
    return true;
}

std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> getSparseContourCos(CameraInformationPtr camera_info, int n)
{
    std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> contour;
    Eigen::Array2d point;
    double u_max = camera_info->width_ - 1.0;
    double v_max = camera_info->height_ - 1.0;
    // Top
    for (int i = 0; i < n; ++i)
    {
        point << (cos((double)i / (double)(n-1) * M_PI) + 1.0) * 0.5  * u_max, 0.0;
        contour.push_back(point);
    }
    // Bottom
    for (int i = 0; i < n; ++i)
    {
        point << (cos((double)i / (double)(n-1) * M_PI) + 1.0) * 0.5 * u_max, v_max;
        contour.push_back(point);
    }

    // Left
    for (int i = 1; i < n - 1; ++i)
    {
        point << 0.0, (cos((double)i / (double)(n-1) * M_PI) + 1.0) * 0.5 * v_max;
        contour.push_back(point);
    }
    // Right
    for (int i = 0; i < n - 1; ++i)
    {
        point << u_max, (cos((double)i / (double)(n-1) * M_PI) + 1.0) * 0.5 * v_max;
        contour.push_back(point);
    }
    return contour;
}

std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> getSparseContour(CameraInformationPtr camera_info, int n)
{
    std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> contour;
    Eigen::Array2d point;
    double u_max = camera_info->width_ - 1.0;
    double v_max = camera_info->height_ - 1.0;
    // Top
    for (int i = 0; i < n; ++i)
    {
        point << (double)i / (double)(n-1) * u_max, 0.0;
        contour.push_back(point);
    }
    // Bottom
    for (int i = 0; i < n; ++i)
    {
        point << (double)i / (double)(n-1) * u_max, v_max;
        contour.push_back(point);
    }

    // Left
    for (int i = 1; i < n - 1; ++i)
    {
        point << 0.0, (double)i / (double)(n-1) * v_max;
        contour.push_back(point);
    }
    // Right
    for (int i = 0; i < n - 1; ++i)
    {
        point << u_max, (double)i / (double)(n-1) * v_max;
        contour.push_back(point);
    }
    return contour;
}

/** @brief 補正前の画像座標から、補正後のポリゴンの頂点を作成
 * @param [in]	Qa	ジャイロの角速度から計算したカメラの方向を表す回転クウォータニオン時系列データ、参照渡し
 * @param [in]	Qf	LPFを掛けて平滑化した回転クウォータニオンの時系列データ、参照渡し
 * @param [in]	m	画面の縦の分割数[ ]
 * @param [in]	n	画面の横の分割数[ ]
 * @param [in]	IK	"逆"歪係数(k1,k2,p1,p2)
 * @param [in]	matIntrinsic	カメラ行列(fx,fy,cx,cy) [pixel]
 * @param [in]	imageSize	フレーム画像のサイズ[pixel]
 * @param [in]  adjustmentQuaternion 画面方向を微調整するクォータニオン[rad]
 * @param [out]	vecPorigonn_uv	OpenGLのポリゴン座標(u',v')座標(-1~1)の組、歪補正後の画面を分割した時の一つ一つのポリゴンの頂点の組
 * @param [in]	zoom	倍率[]。拡大縮小しないなら1を指定すること。省略可
 * @retval true:成功 false:折り返し発生で失敗
 **/
void getUndistortUnrollingContour(
    double ratio,
    MatrixPtr R,
    std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> &contour,
    double zoom,
    CameraInformationPtr camera_info)
{


    //手順
    //1.補正前画像を分割した時の分割点の座標(pixel)を計算
    //2.1の座標を入力として、各行毎のW(t1,t2)を計算
    //3.補正後の画像上のポリゴン座標(pixel)を計算、歪み補正も含める
    double &line_delay = camera_info->line_delay_;
    Eigen::Array2d f, c;
    f << camera_info->fx_, camera_info->fy_;
    c << camera_info->cx_, camera_info->cy_;
    const double &ik1 = camera_info->inverse_k1_;
    const double &ik2 = camera_info->inverse_k2_;
    const double &ip1 = camera_info->inverse_p1_;
    const double &ip2 = camera_info->inverse_p2_;

    contour.clear();
    std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> src_contour = getSparseContourCos(camera_info, 9);
    
    Eigen::Array2d x1;
    Eigen::Vector3d x3, xyz;
    for (auto &p : src_contour)
    {
        x1 = (p - c) / f;
        double r = x1.matrix().norm();
        Eigen::Array2d x2 = x1 * (1.0 + ik1 * pow(r, 2.0) + ik2 * pow(r, 4.0));
        x2[0] += 2.0 * ip1 * x1[0] * x1[1] + ip2 * (pow(r, 2.0) + 2 * pow(x1[0], 2.0));
        x2[1] += ip1 * (pow(r, 2.0) + 2.0 * pow(x1[1], 2.0)) + 2.0 * ip2 * x1[0] * x1[1];
        //折り返し防止
        if (((x2 - x1).abs() > 1).any())
        {
            printf("Warning: Turn backing.\n");
            x2 = x1;
        }
        x3 << x2[0], x2[1], 1.0;
        Eigen::Quaterniond q(Eigen::Map<Eigen::Matrix<float, 3, 3, Eigen::RowMajor>>(&(*R)[std::round(p[1]) * 9], 3, 3).cast<double>());
        Eigen::Vector3d vec = Quaternion2Vector(q) * ratio;
        Eigen::Quaterniond q2 = Vector2Quaternion<double>(vec );

        xyz = q2.normalized().toRotationMatrix() * x3; // Use rotation matrix of each row.
        x2 << xyz[0] / xyz[2], xyz[1] / xyz[2];
        contour.push_back(x2 * f * zoom + c);
    }
}


double bisectionMethod(double zoom,
                         MatrixPtr R,
                         CameraInformationPtr camera_info,
                         double min,
                         double max,
                         int max_iteration, double eps)
{
    std::vector<Eigen::Array2d, Eigen::aligned_allocator<Eigen::Array2d>> contour_a,contour_m;
    getUndistortUnrollingContour(1.0,R,contour_a,zoom,camera_info);

    if(isGoodWarp(contour_a,camera_info))
    {
        return 1.0;
    }

    double a = min;
    double b = max;
    int count = 0;
    double m = 0;
    while ((fabs(a - b) > eps) && (count++ < max_iteration))
    {
        m = (a + b) * 0.5;
        // 1. ここでスパースな輪郭を生成
        
        getUndistortUnrollingContour(a,R,contour_a,zoom,camera_info);
        getUndistortUnrollingContour(m,R,contour_m,zoom,camera_info);
        // 2. 二分法で計算する補正量を制限するパラメータを受け取り、補正後のスパースな輪郭を計算
        // 3. スパースな輪郭をhasBlackSpaceに渡して評価


        if (isGoodWarp(contour_a,camera_info) ^ isGoodWarp(contour_m,camera_info))
        {
            b = m;
        }
        else
        {
            a = m;
        }
        if (count == max_iteration)
        {
            std::cout << "max_iteration" << std::endl;
        }
    }
    return m;
}

