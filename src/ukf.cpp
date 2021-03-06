#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {

  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 0.3; // PROBABLY NEEDS TO BE 0.2

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.3; // PROBABLY NEEDS TO BE 0.2
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.
  
  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

  is_initialized_ = false;
  n_x_ = 5;
  n_aug_ = 7;
  lambda_ = 3 - n_x_;
  n_sig_ = 2 * n_aug_ + 1;
  Xsig_pred_ = MatrixXd(n_x_, n_sig_);
  Xsig_aug_ = MatrixXd(n_aug_, n_sig_);
  weights_ = VectorXd(n_sig_);

  // set weights
  weights_.fill(0.5 / (lambda_ + n_aug_));
  weights_(0) = lambda_ / (lambda_ + n_aug_);

  R_laser_ = MatrixXd(2, 2);
  R_laser_.fill(0);
  R_laser_ << std_laspx_*std_laspx_, 0,
	  0, std_laspy_*std_laspy_;

  //add measurement noise covariance matrix
  R_radar_ = MatrixXd(3, 3);
  R_radar_.fill(0);
  R_radar_ << std_radr_*std_radr_, 0, 0,
	  0, std_radphi_*std_radphi_, 0,
	  0, 0, std_radrd_*std_radrd_;

  laser_nis_counter = 0;
  radar_nis_counter = 0;
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
	if (!is_initialized_) {
		x_ << 1, 1, 0, 0, 0;
		if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
			float rho = meas_package.raw_measurements_(0);
			float phi = meas_package.raw_measurements_(1);
			x_(0) = rho * cos(phi);
			x_(1) = rho * sin(phi);
		}
		else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
			x_(0) = meas_package.raw_measurements_(0);
			x_(1) = meas_package.raw_measurements_(1);
		}
		P_ = MatrixXd::Identity(n_x_, n_x_);
		is_initialized_ = true;
		time_us_ = meas_package.timestamp_;
		return;
	}

	// PREDICTION PART
	dt_ = (meas_package.timestamp_ - time_us_) / 1000000.0;
	time_us_ = meas_package.timestamp_;
	Prediction(meas_package);

	// MEASUREMENT PART
	int meas_size_ = meas_package.raw_measurements_.size();
	z_ = VectorXd(meas_size_);
	S_ = MatrixXd(meas_size_, meas_size_);
	if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
		PredictRadarMeasurement(&z_, &S_);
		UpdateRadar(meas_package.raw_measurements_, &x_, &P_);
	}
	else {
		PredictLidarMeasurement(&z_, &S_);
		UpdateLidar(meas_package.raw_measurements_, &x_, &P_);
	}
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
	AugmentedSigmaPoints(&Xsig_aug_); // IT IS PASSIBLE THAT I NEED TO PASS HERE THE SIG_PRED, AND JUST UPDATE IT
	SigmaPointPrediction(&Xsig_pred_);
	PredictMeanAndCovariance(&x_, &P_);
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
}


void UKF::GenerateSigmaPoints(MatrixXd* Xsig_out) {

	//create sigma point matrix
	MatrixXd Xsig = MatrixXd(n_x_, 2 * n_x_ + 1);

	//calculate square root of P
	MatrixXd A = P_.llt().matrixL();

	//set first column of sigma point matrix
	Xsig.col(0) = x_;

	//set remaining sigma points
	for (int i = 0; i < n_x_; i++)
	{
		Xsig.col(i + 1) = x_ + sqrt(lambda_ + n_x_) * A.col(i);
		Xsig.col(i + 1 + n_x_) = x_ - sqrt(lambda_ + n_x_) * A.col(i);
	}

	//write result
	*Xsig_out = Xsig;
}

void UKF::AugmentedSigmaPoints(MatrixXd* Xsig_out) {

	//create augmented mean vector
	VectorXd x_aug = VectorXd(7);

	//create augmented state covariance
	MatrixXd P_aug = MatrixXd(7, 7);

	//create sigma point matrix
	MatrixXd Xsig_aug = MatrixXd(n_aug_, n_sig_);

	//create augmented mean state
	x_aug.head(5) = x_;
	x_aug(5) = 0;
	x_aug(6) = 0;

	//create augmented covariance matrix
	P_aug.fill(0.0);
	P_aug.topLeftCorner(5, 5) = P_;
	P_aug(n_x_, n_x_) = std_a_*std_a_;
	P_aug(n_x_ + 1, n_x_ + 1) = std_yawdd_*std_yawdd_;

	//create square root matrix
	MatrixXd L = P_aug.llt().matrixL();

	//create augmented sigma points
	Xsig_aug.col(0) = x_aug;
	for (int i = 0; i< n_aug_; i++)
	{
		Xsig_aug.col(i + 1) = x_aug + sqrt(lambda_ + n_aug_) * L.col(i);
		Xsig_aug.col(i + 1 + n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * L.col(i);
	}

	*Xsig_out = Xsig_aug;
}

void UKF::SigmaPointPrediction(MatrixXd* Xsig_out) {

	MatrixXd Xsig_pred = MatrixXd(n_x_, n_sig_);

	//predict sigma points
	for (int i = 0; i< n_sig_; i++)
	{
		//extract values for better readability
		double p_x = Xsig_aug_(0, i);
		double p_y = Xsig_aug_(1, i);
		double v = Xsig_aug_(2, i);
		double yaw = Xsig_aug_(3, i);
		double yawd = Xsig_aug_(4, i);
		double nu_a = Xsig_aug_(5, i);
		double nu_yawdd = Xsig_aug_(6, i);

		//predicted state values
		double px_p, py_p;

		//avoid division by zero
		if (fabs(yawd) > 0.001) {
			px_p = p_x + v / yawd * (sin(yaw + yawd*dt_) - sin(yaw));
			py_p = p_y + v / yawd * (cos(yaw) - cos(yaw + yawd*dt_));
		}
		else {
			px_p = p_x + v*dt_*cos(yaw);
			py_p = p_y + v*dt_*sin(yaw);
		}

		double v_p = v;
		double yaw_p = yaw + yawd*dt_;
		double yawd_p = yawd;

		//add noise
		px_p = px_p + 0.5*nu_a*dt_*dt_ * cos(yaw);
		py_p = py_p + 0.5*nu_a*dt_*dt_ * sin(yaw);
		v_p = v_p + nu_a*dt_;

		yaw_p = yaw_p + 0.5*nu_yawdd*dt_*dt_;
		yawd_p = yawd_p + nu_yawdd*dt_;

		//write predicted sigma point into right column
		Xsig_pred_(0, i) = px_p;
		Xsig_pred_(1, i) = py_p;
		Xsig_pred_(2, i) = v_p;
		Xsig_pred_(3, i) = yaw_p;
		Xsig_pred_(4, i) = yawd_p;
	}
	*Xsig_out = Xsig_pred_;
}

void UKF::PredictMeanAndCovariance(VectorXd* x_out, MatrixXd* P_out) {

	//predicted state mean
	x_.fill(0.0);
	x_ = Xsig_pred_ * weights_;

	//predicted state covariance matrix
	P_.fill(0.0);
	for (int i = 0; i < n_sig_; i++) {  //iterate over sigma points

											   // state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;
		//angle normalization
		while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
		while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

		P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
	}

	//write result
	*x_out = x_;
	*P_out = P_;
}

void UKF::PredictRadarMeasurement(VectorXd* z_out, MatrixXd* S_out) {

	//set measurement dimension, radar can measure r, phi, and r_dot
	int n_z = 3;

	//create matrix for sigma points in measurement space
	Zsig_ = MatrixXd(n_z, n_sig_);
	Zsig_.fill(0);

	//transform sigma points into measurement space
	for (int i = 0; i < n_sig_; i++) {  //2n+1 simga points

											   // extract values for better readibility
		double p_x = Xsig_pred_(0, i);
		double p_y = Xsig_pred_(1, i);
		double v = Xsig_pred_(2, i);
		double yaw = Xsig_pred_(3, i);

		double v1 = cos(yaw)*v;
		double v2 = sin(yaw)*v;

		// measurement model
		Zsig_(0, i) = sqrt(p_x*p_x + p_y*p_y);                        //r
		Zsig_(1, i) = atan2(p_y, p_x);                                 //phi
		Zsig_(2, i) = (p_x*v1 + p_y*v2) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
	}

	//mean predicted measurement
	VectorXd z_pred = VectorXd(n_z);
	z_pred.fill(0.0);
		z_pred = Zsig_ * weights_;

	//innovation covariance matrix S
	MatrixXd S = MatrixXd(n_z, n_z);
	S.fill(0.0);
	for (int i = 0; i < n_sig_; i++) {  //2n+1 simga points
											   //residual
		VectorXd z_diff = Zsig_.col(i) - z_pred;

		//angle normalization
		while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
		while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

		S = S + weights_(i) * z_diff * z_diff.transpose();
	}

	S = S + R_radar_;

	*z_out = z_pred;
	*S_out = S;
}

void UKF::UpdateRadar(VectorXd z_true_, VectorXd* x_out, MatrixXd* P_out) {

	int n_z = 3;

	//create matrix for cross correlation Tc
	MatrixXd Tc = MatrixXd(n_x_, n_z);

	//calculate cross correlation matrix
	Tc.fill(0.0);
	for (int i = 0; i < n_sig_; i++) {
		VectorXd z_diff = Zsig_.col(i) - z_;

		//angle normalization
		while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
		while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;
		//angle normalization
		while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
		while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

		Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
	}

	//Kalman gain K;
	MatrixXd K = Tc * S_.inverse();

	//residual
	VectorXd z_diff = z_true_ - z_;

	//angle normalization
	while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
	while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

	double nis = z_diff.transpose() * S_.inverse() * z_diff;
	cout << "nis radar: " << nis << endl;
	if (nis > 7.851) {
		radar_nis_counter = radar_nis_counter + 1;
	}
	cout << "nis radar bereach: " << radar_nis_counter << endl;

	//update state mean and covariance matrix
	x_ = x_ + K * z_diff;
	P_ = P_ - K*S_*K.transpose();

	//write result
	*x_out = x_;
	*P_out = P_;
}

void UKF::PredictLidarMeasurement(VectorXd* z_out, MatrixXd* S_out) {

	//set measurement dimension, radar can measure r, phi, and r_dot
	int n_z = 2;

	Zsig_ = Xsig_pred_.block(0, 0, n_z, n_sig_);

	//mean predicted measurement
	VectorXd z_pred = VectorXd(n_z);
	z_pred.fill(0.0);
	z_pred = Zsig_ * weights_;

	//innovation covariance matrix S
	MatrixXd S = MatrixXd(n_z, n_z);
	S.fill(0.0);
	for (int i = 0; i < n_sig_; i++) {
		VectorXd z_diff = Zsig_.col(i) - z_pred;

		//angle normalization
		while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
		while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

		S = S + weights_(i) * z_diff * z_diff.transpose();
	}

	S = S + R_laser_;

	*z_out = z_pred;
	*S_out = S;
}

void UKF::UpdateLidar(VectorXd z_true_, VectorXd* x_out, MatrixXd* P_out) {
	int n_z = 2;

	//create matrix for cross correlation Tc
	MatrixXd Tc = MatrixXd(n_x_, n_z);

	//calculate cross correlation matrix
	Tc.fill(0.0);
	for (int i = 0; i < n_sig_; i++) { 
		VectorXd z_diff = Zsig_.col(i) - z_;

		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;
		//angle normalization
		while (x_diff(3)> M_PI) x_diff(3) -= 2.*M_PI;
		while (x_diff(3)<-M_PI) x_diff(3) += 2.*M_PI;

		Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
	}

	//Kalman gain K;
	MatrixXd K = Tc * S_.inverse();

	//residual
	VectorXd z_diff = z_true_ - z_;
	//angle normalization
	// while (z_diff(1)> M_PI) z_diff(1) -= 2.*M_PI;
	// while (z_diff(1)<-M_PI) z_diff(1) += 2.*M_PI;

	double nis = z_diff.transpose() * S_.inverse() * z_diff;
	cout << "nis laser: " << nis << endl;
	if (nis > 5.991) {
		laser_nis_counter = laser_nis_counter + 1;
	}
	cout << "nis laser bereach: " << laser_nis_counter << endl;

	//update state mean and covariance matrix
	x_ = x_ + K * z_diff;
	P_ = P_ - K*S_*K.transpose();

	//write result
	*x_out = x_;
	*P_out = P_;
}
