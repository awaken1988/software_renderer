#include <iostream>
#include <string>
#include <thread>
#include <condition_variable>
#include <cmath>
#include <iterator>
#include <Eigen/Core>
#include <Eigen/Geometry> 
#include <SDL.h>
#include <SDL_opengl.h> 
#include <chrono>
#include <render.h>
#include <render_threading.h>
#include "../sdl2_helper.h"

int reterr(int aRet, std::string aMsg)
{
	std::cerr << aMsg << std::endl;
	return aRet;
}

int main(int argc, char* argv[])
{
	constexpr size_t screen_width = 1024;
	constexpr size_t screen_height = 768;

	bool with_threading = false;
	float focaldistance = 16.0f;
	float rotation = 0.0f;
	NsRenderLib::ThreadPool pool;
	const auto cube_triangles = NsRenderLib::generate_cube_lines();
	const std::array<uint32_t, 6> colors = { 0xfe4219, 0x85fe19, 0x19fef7, 0x1062fc, 0x535254, 0x070707 };
	NsRenderLib::Render myRenderer(screen_width, screen_height, 4);
	const int max_rows = 4;
	const int max_cols = 4;


	//we create different render optiop
		//Thus:
		//	- Camera Settings
		//	- PixelShader
		//	- other options: like additional wireframe
	const auto fov = NsRenderLib::tFov(focaldistance, Eigen::Vector2f(40, 40 / myRenderer.aspectRatio()), 40.0f);

	const auto drawopt_normal = NsRenderLib::tDrawOptions()
		.color(0xAFFEE)
		.fov(fov)
		.wireframe(false);

	const auto drawopt_depth_wireframe = NsRenderLib::tDrawOptions()
		.color(0xAFFEE)
		.fov(fov)
		.wireframe(true);

	const auto drawopt_depth_color = NsRenderLib::tDrawOptions()
		.color(0xAFFEE)
		.fov(fov)
		.pixel_shader([](NsRenderLib::tPixelShaderData aData) -> uint32_t {
			return 0x00112233;
		});

	std::vector<NsRenderLib::tDrawOptions> draw_options = { drawopt_normal, drawopt_depth_wireframe, drawopt_depth_color };

	render_helper::start_sdl2_loop(screen_width, screen_height, [&](SDL_Window* window, SDL_GLContext& context, SDL_Renderer* renderer, SDL_Texture* buffer) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type)
			{
			case SDL_QUIT:
				return 1;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					//case SDLK_UP:    focaldistance += 0.2f; break;
					//case SDLK_DOWN:  focaldistance -= 0.2f; break;
				case SDLK_UP:    
					rotation += 0.01f; 
					break;
				case SDLK_DOWN:  
					rotation -= 0.01f; 
					break;
				}
				break;
			}
		}

		
		//render stuff
		{
			myRenderer.clear(0xFFFFFFFF);

			for (int iRow = 0; iRow < max_rows; iRow++) {
				for (int iCol = 0; iCol < max_cols; iCol++) {
					const int col_row_idx = iCol + (max_cols * iRow);

					auto curr_draw_opt = draw_options[col_row_idx % (draw_options.size())];

					int tri_idx = 0;
					for (auto iTriangle : cube_triangles) {
						Eigen::Matrix3f aa = Eigen::AngleAxis<float>((2 * 3.1234f) * (rotation), Eigen::Vector3f(1.0f, 1.0f, 1.0f).normalized()).toRotationMatrix();
						Eigen::Matrix4f rotation_matrix;
						rotation_matrix.setIdentity();
						rotation_matrix.block<3, 3>(0, 0) = aa;

						const float translation_x = iCol * 4.0f - (max_cols*1.5f);
						const float translation_y = iRow * 4.0f - (max_rows * 1.5f);
						Eigen::Matrix4f translation_matrix = Eigen::Matrix4f::Identity();
						translation_matrix.col(3).head<3>() << translation_x, translation_y, 8.0f;

						const Eigen::Matrix4f world_matrix = translation_matrix * rotation_matrix;

						auto fun = [iTriangle, world_matrix, tri_idx, &colors, &myRenderer, &curr_draw_opt]() -> void {
							auto copy_opt = curr_draw_opt;
							copy_opt.color(colors[tri_idx % std::size(colors)]);

							auto curr_triangle = iTriangle;

							for (auto& iEdge : curr_triangle) {
								iEdge = world_matrix * iEdge;
								//iEdge[2] += 4.0f;
							}

							myRenderer.drawTriangle(curr_triangle, copy_opt);
						};

						if (with_threading)
							pool.add(fun);
						else
							fun();

						tri_idx++;
					}

					if (with_threading)
						pool.join();
				}
			}

			void* buff = myRenderer.getBuffer();
			SDL_UpdateTexture(buffer, NULL, myRenderer.getBuffer(), screen_width * sizeof(Uint32));
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, buffer, NULL, NULL);
			SDL_RenderPresent(renderer);
	
			return 0;
		}
	});

	return 0;
}























//{
//while (true) {
//	NsRenderLib::ThreadPool pool{};
//	for (int i = 0; i < 1000; i++) {
//		pool.add([i]() -> void {
//			std::cout << "    thread " << i << std::endl;
//		});
//	}
//}
//
//return 0;
//	}



//threading class benchmark
//for (int iRound = 0; iRound < 2; iRound++)
//{
//	std::array<float, 10> results = { 0,0,0,0,0,0,0,0,0,0 };
//
//	NsRenderLib::ThreadPool pool{};
//	auto update_time = std::chrono::steady_clock::now();
//	for (int i = 0; i < 10; i++) {
//		auto fun = [i, &results]() -> void {
//			float sum = results[i];
//			for (int j = 0; j < 0xFFFFFF; j++) {
//				sum += sum * 2 + std::sin(sum) + std::cos(sum) + std::tan(sum);
//				sum += std::sin(sum + 0.1f) + std::cos(sum + 0.1f) + std::tan(sum + 0.1f);
//			}
//			results[i] = sum;
//		};
//
//		if (iRound)
//			pool.add(fun);
//		else
//			fun();
//
//	}
//
//	if (iRound)
//		pool.join();
//
//	const auto end_time = std::chrono::steady_clock::now();
//	const auto diff_time = end_time - update_time;
//	const auto diff_time_sec = std::chrono::duration<double, std::milli>(diff_time).count();
//	std::cout << "time " << diff_time_sec << " milliseconds" << std::endl;
//}
//return 0;