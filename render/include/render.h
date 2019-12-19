#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <memory>
#include <array>
#include <thread>
#include <limits>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include "render_threading.h"

namespace NsRenderLib
{
	using namespace Eigen;
	using namespace std;

	constexpr float PI = 3.14159265359f;
	constexpr float DEFAULT_DEPT = 0.0f;
	constexpr float MAX_DEPT = 1.0f;

	constexpr float deg_to_rad(float aDegAngle)
	{
		return (PI * aDegAngle) / 180.0f;
	}

	//Field of view
	struct tFov {
		tFov();
		tFov(float aNearDistance, Vector2f aNearPlane, float aFarDisance);

		float near_distance = 2.0f;
		Vector2f near_plane = Vector2f(200, 200);
		float far_distance = 100.0f;
	};

	//Structure of user overwriteable PixelShader
	struct tPixelShaderData
	{
		tPixelShaderData();
		tPixelShaderData(uint32_t aColor, Vector3f aNormal, Vector2f aProjectedPixel, uint32_t aWidth, uint32_t aHeight);
		~tPixelShaderData();

		uint32_t color;
		Vector3f normal;
		Vector2f projected_pixel;
		uint32_t width;
		uint32_t height;
	};

	typedef std::function < uint32_t(tPixelShaderData)> funcPixelShader;

	//Option each Draw function accept
	struct tDrawOptions
	{
		tDrawOptions();
		tDrawOptions& pixel_shader(funcPixelShader aFunc);
		tDrawOptions& color(uint32_t aColor);
		tDrawOptions& fov(tFov aFov);
		tDrawOptions& wireframe(bool aWireframe);

		optional<funcPixelShader> m_pixelshader;
		optional<uint32_t> m_color;
		optional<tFov> m_fov;
		bool m_wireframe = false;
	};

	class Render
	{
	public:
		Render(uint32_t aWidth, uint32_t aHeight, uint32_t aColorBytes);

		void drawTriangle(vector<Vector4f> aVertices, const tDrawOptions& aDrawOptions);
		void drawTriangle(array<Vector4f, 3> aVertices, const tDrawOptions& aDrawOptions);
		void drawTriangle(Vector4f* aVertices, const tDrawOptions& aDrawOptions);

		void foreachPixel(std::function<void(uint32_t, uint32_t)> aFunc);
		void clear(uint32_t aColor = 0xFFFFFFFF);
		void* getBuffer();

		//misc info
		uint32_t width();
		uint32_t height();
		float aspectRatio();

	protected:
		//stub
		void impl_drawTriangleFilled(const Vector4f* aVertices, const tDrawOptions& aDrawOptions) {
			//impl_drawTriangleFilled_barycentric(aVertices, aDrawOptions);
			impl_drawTriangleFilled_breseham_like(aVertices, aDrawOptions);
		}
		void impl_drawTriangleFilled_barycentric(const Vector4f* aVertices, const tDrawOptions& aDrawOptions);
		void impl_drawTriangleFilled_breseham_like(const Vector4f* aVertices, const tDrawOptions& aDrawOptions);
		void impl_setPixel(const Vector4f& aVertice, uint32_t aColor);
		void impl_drawLine(const Vector4f& aVertice0, const Vector4f& aVertice1, uint32_t aColor);


	protected:
		uint32_t getPixelIndex(uint32_t aX, uint32_t aY);
		bool projectPoint(Vector4f& aPoint, const tDrawOptions& aDrawOptions);
		float withOverHeight();
		float normalized_depth(float aZ) const;

	protected:
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_color_bytes;

		//Buffers
		struct tRenderBuffer {
			std::vector<uint32_t> color;
			std::vector<float> depth;
			std::vector<mutex*> mutex;
		} m_buffer;

		//Defaults
		uint32_t m_default_color;
		tFov m_default_fov;

	};

	vector<std::array<Vector4f, 3>> generate_cube_lines();
}
