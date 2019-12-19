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

int reterr(int aRet, std::string aMsg)
{
	std::cerr << aMsg << std::endl;
	return aRet;
}

int main(int argc, char* argv[])
{
	// Initialize SDL with video
	SDL_Init(SDL_INIT_VIDEO);

	// Create an SDL window
	SDL_Window* window = SDL_CreateWindow("Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_OPENGL);
	if (!window) {
		return reterr(1, "Error failed to create window!");
	}

	// Create an OpenGL context (so we can use OpenGL functions)
	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (!context) {
		return reterr(2, "Error failed to create a context!");
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (!renderer) {
		return reterr(3, "Could not create renderer");
	}

	SDL_Texture* buffer = SDL_CreateTexture(renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STATIC,
		640,
		480);

	bool quit = false;
	int frame = 0;
	int offset = 1;

	auto framecounter_time = std::chrono::steady_clock::now();
	auto update_time = std::chrono::steady_clock::now();
	NsRenderLib::Render myRenderer(640, 480, 4);
	float focaldistance = 8.0f;
	constexpr bool with_threading = false;

	NsRenderLib::ThreadPool pool;

	float rotation = 0.0f;

	while ( !quit ) {

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type)
			{
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
				//case SDLK_UP:    focaldistance += 0.2f; break;
				//case SDLK_DOWN:  focaldistance -= 0.2f; break;
				case SDLK_UP:    rotation += 0.01f; break;
				case SDLK_DOWN:  rotation -= 0.01f; break;
				}
				break;
			}
		}	

		const auto draw_opt = NsRenderLib::tDrawOptions()
			.pixel_shader([](NsRenderLib::tPixelShaderData aData) -> uint32_t {
				auto color = aData.color;
				uint8_t* color_array = reinterpret_cast<uint8_t*>(&color);
				const auto angle = aData.normal.dot(Eigen::Vector3f(1.0f, 1.0f, 1.0f).normalized());


				//std::cout << aData.projected_pixel.x() << std::endl;

				const float x = aData.projected_pixel.x() / aData.width;
				const float y = aData.projected_pixel.y() / aData.height;


				const bool is_invert_x = ((int)std::abs(x / 0.02f)) % 2 == 0;
				const bool is_invert_y = ((int)std::abs(y / 0.02f)) % 2 == 0;
				const bool is_invert = is_invert_y ? is_invert_x : !is_invert_x;

				if (is_invert) {
					color = 0x111111;
					//std::cout << std::hex << color << std::endl;
				}
				else {
					color = 0xAAAAAA;
				}

				//color_array[0] *= angle;

				return color; })
			.color(0xAFFEE)
			.fov(NsRenderLib::tFov(focaldistance, Eigen::Vector2f(40, 40 / myRenderer.aspectRatio()), 10.0f))
			.wireframe(false);

		//render stuff
		{
			myRenderer.clear(0xFFFFFFFF);

			Eigen::Matrix3f aa = Eigen::AngleAxis<float>((2 * 3.1234f) * (rotation), Eigen::Vector3f(1.0f, 1.0f, 1.0f).normalized()).toRotationMatrix();
			Eigen::Matrix4f rotation;
			rotation.setIdentity();
			rotation.block<3, 3>(0, 0) = aa;
			
			//std::cout << rotation << std::endl;

			auto cube_triangles = NsRenderLib::generate_cube_lines();

			std::array<uint32_t, 6> colors = { 0xfe4219, 0x85fe19, 0x19fef7, 0x1062fc, 0x535254, 0x070707 };

			int tri_idx = 0;
			for (auto iTriangle : cube_triangles) {
				for (auto& iEdge : iTriangle) {
					iEdge = rotation * iEdge;
					iEdge.z() += 4.0f;	

					//Eigen::IOFormat CommaInitFmt(Eigen::StreamPrecision, Eigen::DontAlignCols, ", ", ", ", "", "", " << ", ";");
					//std::cout << iEdge.format(CommaInitFmt) << std::endl;
				}

				auto fun = [iTriangle, tri_idx, &colors, &myRenderer, &draw_opt]() -> void {
					auto copy_opt = draw_opt;

					copy_opt.color(colors[tri_idx % std::size(colors)]);

					myRenderer.drawTriangle(iTriangle, copy_opt);
				};

				if (with_threading)
					pool.add(fun);
				else
					fun();

				tri_idx++;

				break;
			}

			if(with_threading)
				pool.join();

			void* buff = myRenderer.getBuffer();

			

			SDL_UpdateTexture(buffer, NULL, myRenderer.getBuffer(), 640 * sizeof(Uint32));

			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, buffer, NULL, NULL);
			SDL_RenderPresent(renderer);

			
		}

		frame++;
		const auto end_time = std::chrono::steady_clock::now();
		//frametime
		{
			auto diff_time = end_time - framecounter_time;
			auto diff_time_sec = std::chrono::duration<double, std::milli>(diff_time).count();

			if (diff_time_sec > 1000.0) {
				std::string caption = std::to_string(frame) + " Frames";
				SDL_SetWindowTitle(window, caption.c_str());
				frame = 0;

				framecounter_time = std::chrono::steady_clock::now();

				
			}
		}

		//updatetime
		{
			auto diff_time = end_time - update_time;
			auto diff_time_sec = std::chrono::duration<double, std::milli>(diff_time).count();

			if (diff_time_sec > 10.0) {
				offset += 1;
				update_time = std::chrono::steady_clock::now();
			}
		}
	}


	// Destroy the context
	SDL_GL_DeleteContext(context);

	// Destroy the window
	SDL_DestroyWindow(window);

	// And quit SDL
	SDL_Quit();

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