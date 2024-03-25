// -*-c++-*---------------------------------------------------------------------------------------
// Copyright 2024 Bernd Pfrommer <bernd.pfrommer@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <tagslam/pose_noise.hpp>

namespace tagslam
{

// static function
PoseNoise PoseNoise::make(const Point3d & a, const Point3d & p)
{
  Matrix6d m = Matrix6d::Zero();
  m.diagonal() << a(0) * a(0), a(1) * a(1), a(2) * a(2), p(0) * p(0),
    p(1) * p(1), p(2) * p(2);
  return (PoseNoise(m, true));
}

// static function
PoseNoise PoseNoise::make(double a, double p)
{
  return (make(Point3d(a, a, a), Point3d(p, p, p)));
}

static const PoseNoise::Matrix6d sqrt_info_to_sigma(
  const PoseNoise::Matrix6d & R)
{
  // TODO: test this! Is it working at all???
  const PoseNoise::Matrix6d rsqi = (R.transpose() * R).inverse();
  return (rsqi);
}

// static method
PoseNoise PoseNoise::makeFromR(const Matrix6d & R)
{
  const Matrix6d sigma = sqrt_info_to_sigma(R);
  return (PoseNoise(sigma, false /*isdiag*/));
}

Eigen::Matrix<double, 6, 1> PoseNoise::getDiagonal() const
{
  return (noise_.diagonal());
}

const PoseNoise::Matrix6d PoseNoise::getCovarianceMatrix() const
{
  return (noise_);
}

PoseNoise::Matrix6d PoseNoise::convertToR() const
{
  //Sigma = R * R^T
  // Cholesky decomposition: sigma = L * L.transpose();
  const Matrix6d ni = noise_.inverse();
  Eigen::LLT<Matrix6d> llt(ni);
  const Matrix6d U = llt.matrixU();
  return (U);
}

std::ostream & operator<<(std::ostream & os, const PoseNoise & pn)
{
  os << "is_diagonal: " << pn.isDiagonal_ << std::endl << pn.noise_;
  return (os);
}

}  // namespace tagslam
