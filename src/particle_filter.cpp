#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <limits>
#include <map>
#include "particle_filter.h"
using namespace std;


void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of
	//   x, y, theta and their uncertainties from GPS) and all weights to 1.
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).
	default_random_engine gen;

	// Gaussian normal distribution with mean and std for x, y and theta
	normal_distribution<double> dist_x(x, std[0]);
	normal_distribution<double> dist_y(y, std[1]);
	normal_distribution<double> dist_psi(theta, std[2]);

	num_particles = 25;
	particles.resize(num_particles);
	for (int i=0; i < num_particles; i++){
		Particle p;
		p.id = i;
		p.x = dist_x(gen);
		p.y = dist_y(gen);
		p.theta = dist_psi(gen);
		p.weight = 1.0; //initialize all particle weights to 1
		particles[i] = p;
	}

	weights.resize(num_particles);
	is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/
	default_random_engine gen;
	double x, y, theta;

	for (auto& p:particles) {
		if (fabs(yaw_rate) < 1e-8) {
			x = p.x + velocity * delta_t * cos(p.theta);
			y =  p.y + velocity * delta_t * sin(p.theta);
			theta = p.theta;
		} else {
			x = p.x + velocity/yaw_rate * (sin(p.theta + yaw_rate * delta_t) - sin(p.theta));
			y = p.y + velocity/yaw_rate * (cos(p.theta) - cos(p.theta + yaw_rate * delta_t));
			theta = yaw_rate * delta_t + p.theta;
		}

        normal_distribution<double> dist_x(x, std_pos[0]);
        normal_distribution<double> dist_y(y, std_pos[1]);
        normal_distribution<double> dist_yaw(theta, std_pos[2]);

        p.x = dist_x(gen);
        p.y = dist_y(gen);
        p.theta = dist_yaw(gen);
    }
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to
	//   implement this method and use it as a helper during the updateWeights phase.
	for (auto& o:observations) {
		double min_distance = std::numeric_limits<float>::max();
		for (const auto& p:predicted) {
			double distance = dist(p.x, p.y, o.x, o.y);
			if (distance < min_distance) {
				min_distance = distance;
				o.id = p.id;
			}
		}
	}
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
		std::vector<LandmarkObs> observations, Map map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation
	//   3.33. Note that you'll need to switch the minus sign in that equation to a plus to account
	//   for the fact that the map's y-axis actually points downwards.)
	//   http://planning.cs.uiuc.edu/node99.html
	vector<LandmarkObs> transformed_observations;
  	transformed_observations.resize(observations.size());

	for (int i=0; i < num_particles; ++i) {
		Particle &particle = particles[i];

	  	for (int j=0; j < observations.size(); ++j) {
		  	LandmarkObs transformed_ob;
			// transform from vehicle coordinate to map coordinate
			transformed_ob.x = observations[j].x * cos(particle.theta) - observations[j].y * sin(particle.theta) + particle.x;
			transformed_ob.y = observations[j].x * sin(particle.theta) + observations[j].y * cos(particle.theta) + particle.y;
			transformed_observations[j] = transformed_ob;
		}

		std::vector<LandmarkObs> inrange_landmarks;
		std::map<int, Map::single_landmark_s> idx2landmark;

		for (const auto& landmark:map_landmarks.landmark_list){
			double distance = dist(landmark.x_f, landmark.y_f, particle.x, particle.y);
			if (distance <= sensor_range){
				inrange_landmarks.push_back(LandmarkObs{ landmark.id_i,landmark.x_f,landmark.y_f });
				idx2landmark.insert(std::make_pair(landmark.id_i, landmark));
			}
		}

	  if (inrange_landmarks.size() > 0) {
		  dataAssociation(inrange_landmarks, transformed_observations);
		  particle.weight = 1.0; //reset the weight of the particle
		  for (const auto observation:transformed_observations) {
			  double mu_x = idx2landmark[observation.id].x_f;
			  double mu_y = idx2landmark[observation.id].y_f;
			  double x = observation.x;
			  double y = observation.y;
			  double s_x = std_landmark[0];
			  double s_y = std_landmark[1];
			  double x_diff = (x - mu_x) * (x - mu_x) / (2 * s_x * s_x);
			  double y_diff = (y - mu_y) * (y - mu_y) / (2 * s_y * s_y);
			  particle.weight *= 1 / (2 * M_PI * s_x * s_y) * exp(-(x_diff + y_diff));
		  }
		  weights[i] = particle.weight;
	  } else {
		  weights[i] = 0.0;
	  }
	}
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight.
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution

	std::random_device rd;
	std::mt19937 gen(rd());
    std::discrete_distribution<> d(weights.begin(), weights.end());
    vector<Particle> particles_new;

    particles_new.resize(num_particles);
    for (int i=0; i < num_particles; i++) {
        particles_new[i] = particles[d(gen)];
    }

    particles = particles_new;
}

void ParticleFilter::write(std::string filename) {
	// You don't need to modify this file.
	std::ofstream dataFile;
	dataFile.open(filename, std::ios::app);
	for (int i = 0; i < num_particles; ++i) {
		dataFile << particles[i].x << " " << particles[i].y << " " << particles[i].theta << "\n";
	}
	dataFile.close();
}
