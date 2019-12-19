#include "render.h"

namespace NsRenderLib
{
	//---------------------------------------------------------
	// DrawOption
	//---------------------------------------------------------
	tDrawOptions::tDrawOptions()
	{

	}

	tDrawOptions& tDrawOptions::pixel_shader(funcPixelShader aFunc) 
	{
		m_pixelshader = aFunc;
		return *this;
	}

	tDrawOptions& tDrawOptions::color(uint32_t aColor)
	{
		m_color = aColor;
		return *this;
	}

	tDrawOptions& tDrawOptions::fov(tFov aFov)
	{
		m_fov = aFov;
		return *this;
	}

	tDrawOptions& tDrawOptions::wireframe(bool aWireframe)
	{
		m_wireframe = aWireframe;
		return *this;
	}


	//---------------------------------------------------------
	// Render
	//---------------------------------------------------------
	Render::Render(uint32_t aWidth, uint32_t aHeight, uint32_t aColorBytes)
		:	m_width(aWidth), m_height(aHeight), 
			m_color_bytes(aColorBytes), 
			m_default_color((~aColorBytes)&0x00FFFFFF),
			m_default_fov(8.0f, Eigen::Vector2f(40, 40 / this->aspectRatio()), 10.0f)
	{
		m_buffer.color.resize(m_width * m_height * m_color_bytes);
		m_buffer.depth.resize(m_width * m_height * sizeof(float));
		
		for (int iPixel = 0; iPixel < m_width * m_height; iPixel++) {
			m_buffer.mutex.push_back( new mutex );
		}

		clear();
	}

	void Render::foreachPixel(std::function<void(uint32_t, uint32_t)> aFunc)
	{
		for (uint32_t iX = 0; iX < m_width; iX++) {
			for (uint32_t iY = 0; iY < m_height; iY++) {
				aFunc(iX, iY);
			}
		}
	}

	void Render::clear(uint32_t aColor)
	{
		foreachPixel([&](uint32_t aX, uint32_t aY) {
			m_buffer.color[getPixelIndex(aX, aY)] = aColor;
			m_buffer.depth[getPixelIndex(aX, aY)] = 1.0f;
		});
	}

	void Render::impl_drawLine(const Vector4f& aVertice0, const Vector4f& aVertice1, uint32_t aColor)
	{
		bool isXAxis = true;

		Vector4f point0 = aVertice0;
		Vector4f point1 = aVertice1;
		float dX = point1.x() - point0.x();
		float dY = point1.y() - point0.y();
		bool isXMain = true;

		if (0.0f == dX && 0.0f == dY) {
			this->impl_setPixel(point0, aColor);
			return;
		}
		else if (abs(dX) < abs(dY)) {
			std::swap(dX, dY);
			std::swap(point0.x(), point0.y());
			std::swap(point1.x(), point1.y());
			isXMain = false;
		}

		if (point0.x() > point1.x()) {
			std::swap(point0, point1);
			dX = -dX;
		}

		float error = dX / 2.0f;
		float ysign = point0.y() < point1.y() ? 1.0f : -1.0f;

		while (point0.x() < point1.x()) {
			if (isXMain) {
				this->impl_setPixel(point0, aColor);
			}
			else {
				this->impl_setPixel(Vector4f(point0.y(), point0.x(), point0.z(), 0.0f), aColor);
			}

			if (error > 0.0f) {
				point0.x() += 1;
				error -= abs(dY);
			}
			else {
				point0.y() += 1 * ysign;
				error += dX;
			}
		}
	}

	void Render::impl_drawTriangleFilled_barycentric(const Vector4f* aVertices, const tDrawOptions& aDrawOptions)
	{
		const Vector3f vertices_3[] = { aVertices[0].head(3), aVertices[1].head(3), aVertices[2].head(3), };

		const Vector3f d01 = (aVertices[1] - aVertices[0]).head(3);
		const Vector3f d02 = (aVertices[2] - aVertices[0]).head(3);
		uint32_t color = aDrawOptions.m_color.value_or(m_default_color);

		const float len01 = d01.head(2).norm();
		const float len02 = d02.head(2).norm();
		const float max_len = len01 > len02 ? len01 : len02;
		const float step = 0.5f / max_len;

		const Vector3f result_normal = vertices_3[0].cross(vertices_3[1]).normalized();

		for (float iU = 0; iU < 1.0f; iU+=step) {
				for (float iV = 0; iV < 1.0f; iV += step) {
					if (iU + iV > 1.0f)
						continue;

					//TODO: interpolate depth
					const Vector3f result = aVertices[0].head(3) + iU * d01 + iV * d02;
					const Vector2f result_xy = result.head(2);

					uint32_t color_from_pixelshader = color;
					
					if (aDrawOptions.m_pixelshader.has_value()) {
						color_from_pixelshader = aDrawOptions.m_pixelshader.value()(tPixelShaderData(color, result_normal, result_xy, m_width, m_height));
					}

					Vector4f tmp;
					tmp[0] = result[0];
					tmp[1] = result[1];
					tmp[2] = result[2];
					tmp[3] = 0;

					impl_setPixel(tmp, color_from_pixelshader);
				}
		}
	}

	void Render::impl_drawTriangleFilled_breseham_like(const Vector4f* aVertices, const tDrawOptions& aDrawOptions)
	{
		//sort 3 arrays indeces by left, middle, right | top, middle, bottom (depends on aAxis)
		auto sorted_idx = [&aVertices](size_t aAxis) -> std::array<size_t, 3> {
			size_t idx_max = 0;
			size_t idx_min = 0;
			for (int iIdx = 0; iIdx < 3; iIdx++) {
				if (aVertices[iIdx][aAxis] > aVertices[idx_max][aAxis])
					idx_max = iIdx;
				if (aVertices[iIdx][aAxis] < aVertices[idx_min][aAxis])
					idx_min = iIdx;
			}

			const size_t idx_middle = (0x3 & ~(0x1 << idx_min | 0x1 << idx_max)) / 2;

			return { idx_min,  idx_middle, idx_max };
		};

		struct tHalfTri {
			Vector4f p0, p1, p2;
			bool is_valid=false;
		};

		const uint32_t color = aDrawOptions.m_color.value_or(m_default_color);
		
		//get left, middle, right and bottom,middle,top vertiex indices
		std::array<size_t, 3> lmr = sorted_idx(0);	
		std::array<size_t, 3> bmt = sorted_idx(1);
		
		//cut triangle -> cut line have to be x axis aligned
		const auto half_triangles = [&]() -> auto {
			array<tHalfTri, 2> ret;

			const Vector4f& top = aVertices[bmt[2]];
			const Vector4f& middle = aVertices[bmt[1]];
			const Vector4f& bottom = aVertices[bmt[0]];

			if (top[1] == middle[1] ) {
				ret[0] = { middle, top, bottom, true };
			}
			else if (bottom[1] == middle[1]) {
				ret[0] = { middle, bottom, top, true };
			}
			else {
				const Vector4f bottom_top = top - bottom;
				const float bottom_mult = (middle[1] - bottom[1]) / bottom_top[1];
				const Vector4f other_middle = bottom + bottom_top * bottom_mult;
				
				ret[0] = { middle, other_middle, top, true };
				ret[1] = { middle, other_middle, bottom, true };
			}

			return ret;
		}();
			
		//draw 1 or 2 triangles (vertical line is x axis aligned)
		for (const tHalfTri& iHalfTri: half_triangles) {
			if (!iHalfTri.is_valid)
				continue;
			if (iHalfTri.p0[1] != iHalfTri.p1[1])
				throw "line p1---p2 have to be horizontal";
			
			const float delta_y_signed = iHalfTri.p2[1] - iHalfTri.p0[1];
			const float sign = delta_y_signed < 0.0f ? -1.0f : 1.0f;
			const float delta_y = delta_y_signed * sign;

			//for flatten triangle
			if ( delta_y < 0.5f ) {
				this->impl_drawLine(iHalfTri.p0, iHalfTri.p1, color);
				continue;
			}

			const float p0_delta = (iHalfTri.p2[0] - iHalfTri.p0[0]) / delta_y_signed;
			const float p1_delta = (iHalfTri.p2[0] - iHalfTri.p1[0]) / delta_y_signed;
			Vector4f curr_p0 = iHalfTri.p0;
			Vector4f curr_p1 = iHalfTri.p1;

			while (curr_p0[1] < iHalfTri.p2[1]) {
				this->impl_drawLine(curr_p0, curr_p1, color);

				curr_p0[1] += sign;
				curr_p0[0] += p0_delta;

				curr_p1[1] += sign;
				curr_p1[0] += p1_delta;
			}

			impl_drawLine(iHalfTri.p0, iHalfTri.p1, 0xFF00);
			impl_drawLine(iHalfTri.p1, iHalfTri.p2, 0xFF00);
			impl_drawLine(iHalfTri.p2, iHalfTri.p0, 0xFF00);
		}

		//for debug
		for(float iDbg=1.5f; iDbg<2.5f; iDbg+=1.1f)
		{
			const Vector4f dbg_vec0 = aVertices[0]* iDbg;
			const Vector4f dbg_vec1 = aVertices[1]* iDbg;
			const Vector4f dbg_vec2 = aVertices[2]* iDbg;
			impl_drawLine(dbg_vec0, dbg_vec1, 0xFF0000);
			impl_drawLine(dbg_vec1, dbg_vec2, 0xFF0000);
			impl_drawLine(dbg_vec2, dbg_vec0, 0xFF0000);
		}

		
	}

	void Render::impl_setPixel(const Vector4f& aVertice, uint32_t aColor)
	{
		if (aVertice.x() >= m_width || aVertice.x() < 0.0f || aVertice.y() >= m_height || aVertice.y() < 0.0f)
			return;

		const auto depth = aVertice.z();
		const auto x = static_cast<size_t>(aVertice.x());
		const auto y = static_cast<size_t>(aVertice.y());
		const auto idx = getPixelIndex(x, y);

		m_buffer.mutex[idx]->lock();
		if (depth <= m_buffer.depth[idx]) {
			m_buffer.color[idx] = aColor  ;
			m_buffer.depth[idx] = depth;
		}
		m_buffer.mutex[idx]->unlock();
	}

	uint32_t Render::getPixelIndex(uint32_t aX, uint32_t aY)
	{
		return (m_width * aY + aX);
	}

	bool Render::projectPoint(Vector4f& aPoint, const tDrawOptions& aDrawOptions)
	{
		const tFov& currFov = aDrawOptions.m_fov.value_or(m_default_fov);

		if (aPoint.z() < 0.0f)
			return false;
		//TODO: implement far clipping plane

		const Vector4f tmp = aPoint;

		//projection
		aPoint = Vector4f(
			(currFov.near_distance / aPoint.z()) * aPoint.x(),
			(currFov.near_distance / aPoint.z()) * aPoint.y(),
			(aPoint.z()),
			0.0f);

		//clip x/y
		if (abs(aPoint.x()) > (currFov.near_plane.x() / 2.0f))
			return false;
		if (abs(aPoint.y()) > (currFov.near_plane.y() / 2.0f))
			return false;

		aPoint.z() = (1.0f * aPoint.z()) / currFov.far_distance;

		//fit to screen
		aPoint.x() *= m_width / currFov.near_plane.x();
		aPoint.y() *= m_height / currFov.near_plane.y();

		//to center of the sceen
		aPoint.x() += m_width / 2.0f;
		aPoint.y() += m_height / 2.0f;

		return true;
	}

	float Render::withOverHeight()
	{
		return static_cast<float>(m_width) / static_cast<float>(m_height);
	}

	float Render::normalized_depth(float aZ) const
	{
		return (1.0f * aZ) / m_default_fov.far_distance;
	}

	void* Render::getBuffer()
	{
		return reinterpret_cast<void*>(m_buffer.color.data());
	}

	void Render::drawTriangle(vector<Vector4f> aVertices, const tDrawOptions& aDrawOptions)
	{
		if (3 != aVertices.size())
			throw "there should be 3 vertices given to drawTriangle";

		drawTriangle(aVertices.data(), aDrawOptions);
	}

	void Render::drawTriangle(array<Vector4f, 3> aVertices, const tDrawOptions& aDrawOptions)
	{
		if (3 != aVertices.size())
			throw "there should be 3 vertices given to drawTriangle";

		drawTriangle(aVertices.data(), aDrawOptions);
	}

	void Render::drawTriangle(Vector4f* aVertices, const tDrawOptions& aDrawOptions)
	{
		Vector4f vertices[] = { aVertices[0], aVertices[1], aVertices[2] };

		for (Vector4f& iPoint : vertices) {
			if (!projectPoint(iPoint, aDrawOptions)) {
				return;
			}
		}

		if (aDrawOptions.m_wireframe) {
			const uint32_t color = aDrawOptions.m_color.value_or(0xDEADBEEF);	//TODO: set default color if not avail
		
			impl_drawLine(vertices[0], vertices[1], color);
			impl_drawLine(vertices[1], vertices[2], color);
			impl_drawLine(vertices[2], vertices[0], color);
		}
		else {
			Render::impl_drawTriangleFilled(vertices, aDrawOptions);
		}
	}

	uint32_t Render::width()
	{
		return m_width;
	}

	uint32_t Render::height()
	{
		return m_height;
	}

	float Render::aspectRatio()
	{
		return static_cast<float>(m_width) / static_cast<float>(m_height);
	}

	vector<std::array<Vector4f, 3>> generate_cube_lines()
	{
		vector<std::array<Vector4f, 3>> ret;

		std::tuple<int, int, int> walking_indices[] = {
			{0, 1, 2},
			{0, 2, 1},
			{1, 2, 0},
		};

		auto rectangle_fun = [](int iEdge, int aWalk0, int aWalk1, int aFixed, bool aIsFront) -> Vector4f {
			std::array<int, 5> value = { 0x0, 0x1, 0x3, 0x2, 0x0, };

			Vector4f edge;
			edge[aWalk0] = (value[iEdge%5] & 0x1) ? 1.0f : -1.0f;
			edge[aWalk1] = (value[iEdge%5] & 0x2) ? 1.0f : -1.0f;
			edge[aFixed] = aIsFront ? 1.0f : -1.0f;
			edge[3] = 0.0f;

			return edge;
		};
	
		for (auto iWalk : walking_indices) {
			const auto walk0 = std::get<0>(iWalk);
			const auto walk1 = std::get<1>(iWalk);
			const auto fixed = std::get<2>(iWalk);

			for (int iSide = 0; iSide < 2; iSide++) {
				for (int iEdge = 0; iEdge < 4; iEdge++) {
					const auto p0 = rectangle_fun(iEdge + 0, walk0, walk1, fixed, iSide);
					const auto p1 = rectangle_fun(iEdge + 1, walk0, walk1, fixed, iSide);
					const auto p2 = rectangle_fun(iEdge + 2, walk0, walk1, fixed, iSide);
					
					ret.push_back({ p0, p1, p2 });
				}
			}
		}

		return ret;
	}

	tFov::tFov() 
	{
	}

	tFov::tFov(float aNearDistance, Vector2f aNearPlane, float aFarDisance)
		: near_distance(aNearDistance), near_plane(aNearPlane), far_distance(aFarDisance) 
	{
	}

	tPixelShaderData::tPixelShaderData()
		: color(0xFFFFFFFF), width(0), height(0)
	{
	}

	tPixelShaderData::tPixelShaderData(uint32_t aColor, Vector3f aNormal, Vector2f aProjectedPixel, uint32_t aWidth, uint32_t aHeight)
		: color(aColor), normal(aNormal), projected_pixel(aProjectedPixel), width(aWidth), height(aHeight)
	{
	}
	
	tPixelShaderData::~tPixelShaderData()
	{
	}
}

