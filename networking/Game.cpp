#include "Game.hpp"
#include <iostream>

bool Game::check_point(glm::vec2 pt) {
  glm::vec2 to = ball - pt;
  float len2 = glm::dot(to, to);
  if (len2 > BallRadius * BallRadius) return false;
  //if point is inside ball, make ball velocity outward-going:
  //float d = glm::dot(ball_velocity, to);
  //ball_velocity += ((std::abs(d) - d) / len2) * to;
  ball_velocity = to * (ball_speed / to.length());
  std::cout << "ballvel: " << ball_velocity.x << " " << ball_velocity.y << std::endl;
  return true;
}

void Game::check_score() {
  if (score1 >= win_score) {
    won = is_player1;
    lost = !is_player1;
  }
  else if (score2 >= win_score) {
    won = !is_player1;
    lost = is_player1;
  }
}

void Game::reset_ball() {
  ball.x = 0.0f;
  ball.y = 0.0f;
  ball_velocity.x = 0.0f;
  ball_velocity.y = 0.0f;
}

void Game::update(float time) {
  if (won || lost) return;

  if (fire1) {
    bullet1.x = -bullet_startx;
    bullet1.y = 0.0f;
    bullet1_velocity.x = glm::cos(glm::radians(paddle1 - 90.0f)) * bullet_speed;
    bullet1_velocity.y = glm::sin(glm::radians(paddle1 - 90.0f)) * bullet_speed;
    std::cout << "fire1" << std::endl;
    fire1 = false;
  }

  if (fire2) {
    bullet2.x = bullet_startx;
    bullet2.y = 0.0f;
    bullet2_velocity.x = glm::cos(glm::radians(paddle2 - 90.0f)) * bullet_speed;
    bullet2_velocity.y = glm::sin(glm::radians(paddle2 - 90.0f)) * bullet_speed;
    std::cout << "fire2" << std::endl;
    fire2 = false;
  }

  bullet1 += bullet1_velocity * time;
  bullet2 += bullet2_velocity * time;
	ball += ball_velocity * time;
	if (ball.x >= 0.5f * FrameWidth - BallRadius) {
    score1++;
    check_score();
    reset_ball();
	}
	if (ball.x <=-0.5f * FrameWidth + BallRadius) {
    score2++;
    check_score();
    reset_ball();
	}
	if (ball.y >= 0.5f * FrameHeight - BallRadius) {
		ball_velocity.y = -std::abs(ball_velocity.y);
	}
	if (ball.y <=-0.5f * FrameHeight + BallRadius) {
		ball_velocity.y = std::abs(ball_velocity.y);
	}

	/*auto do_point = [this](glm::vec2 const &pt) {
		glm::vec2 to = ball - pt;
		float len2 = glm::dot(to, to);
		if (len2 > BallRadius * BallRadius) return;
		//if point is inside ball, make ball velocity outward-going:
		float d = glm::dot(ball_velocity, to);
		ball_velocity += ((std::abs(d) - d) / len2) * to;
	};*/

  if (check_point(bullet1)) {
    bullet1.x = 0.0f;
    bullet1.y = 10.0f;
    bullet1_velocity.x = 0.0f;
    bullet1_velocity.y = 0.0f;
  }
  if (check_point(bullet2)) {
    bullet2.x = 0.0f;
    bullet2.y = 10.0f;
    bullet2_velocity.x = 0.0f;
    bullet2_velocity.y = 0.0f;
  }

	/*auto do_edge = [&](glm::vec2 const &a, glm::vec2 const &b) {
		float along = glm::dot(ball-a, b-a);
		float max = glm::dot(b-a,b-a);
		if (along <= 0.0f || along >= max) return;
		do_point(glm::mix(a,b,along/max));
	};

	do_edge(glm::vec2(paddle.x + 0.5f * PaddleWidth, paddle.y), glm::vec2(paddle.x - 0.5f * PaddleWidth, paddle.y));*/

}
