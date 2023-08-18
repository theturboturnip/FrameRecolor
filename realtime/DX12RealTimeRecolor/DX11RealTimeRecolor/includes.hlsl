// Derived from https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx
// Section: Converting 8-bit YUV to RGB888
// Converts BT.601 YUV to RGB
static const float3x3 YUVtoRGBCoeffMatrix =
{
	1.164383f,  1.164383f, 1.164383f,
	0.000000f, -0.391762f, 2.017232f,
	1.596027f, -0.812968f, 0.000000f
};

// TODO the color primaries for bt601 are slightly different than for srgb, but who's counting
float3 yuv_bt601_to_srgb(float3 yuv)
{
	// Derived from https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx
	// Section: Converting 8-bit YUV to RGB888

	// These values are calculated from (16 / 255) and (128 / 255)
	yuv -= float3(0.062745f, 0.501960f, 0.501960f);
	yuv = mul(yuv, YUVtoRGBCoeffMatrix);

	return saturate(yuv);
}

float rec2020_linearize(float e_prime) {
	float alpha = 1.099, beta = 0.018; // for 10-bit systems

	// E' = (E < beta) ? (4.5 * E) : (alpha * pow(E, 0.45) - (alpha - 1));
	if (e_prime <= beta * 4.5) {
		return e_prime / 4.5;
	}
	else {
		// E' = alpha * pow(E, 0.45) - (alpha - 1)
		// alpha * pow(E, 0.45) = E' + alpha - 1
		// pow(E, 0.45) = (E' + alpha - 1) * alpha
		// 1/0.45 = 2.22223
		// E = pow(pow(E, 0.45), 2.22223) = pow((E' + alpha - 1) * alpha, 2.22223)
		return pow((e_prime + alpha - 1) * alpha, 2.22223);
	}
}

float3 yuv_rec2020_10bit_to_linear_rgb(uint y_enc, uint2 crcb_enc) {
	// u = 2x/(6y - x + 1.5)
	// v = 3y/(6y - x + 1.5)
	// u/v = 2x/3y
	
	// Assume y, cr, cb are 10bit i.e. uints from [0, 1024)
	// Y is coded as int[((219 * Y) + 16) * 2^n-8]
	// assuming n=10 for 10bits? then 
	float y_prime = ((y_enc / 4.0) - 16.0) / 219.0;
	// Cr, Cb are encoded as int[((224 * C) + 128) * 2^n-8]
	float cr = ((crcb_enc.y / 4.0) - 128.0) / 224.0;
	float cb = ((crcb_enc.x / 4.0) - 128.0) / 224.0;

	// From the recommendation specs:
	// https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2020-2-201510-I!!PDF-E.pdf
	// 
	// Y' = 0.2627R' + 0.6780G' + 0.0593B'
	// C'R = (R' - Y')/1.4746
	//     => R' = (1.4746C'R) + Y'
	// C'B = (B' - Y')/1.8814
	//     => B' = (1.8814C'B) + Y'

	float r_prime = (1.4746 * cr) + y_prime;
	float b_prime = (1.8814 * cb) + y_prime;
	float g_prime = (y_prime - 0.2627 * r_prime - 0.0593 * b_prime) / 0.6780;

	return float3(
		rec2020_linearize(r_prime),
		rec2020_linearize(g_prime),
		rec2020_linearize(b_prime)
		//r_prime, g_prime, b_prime
	);
}

// Derived from https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2020-2-201510-I!!PDF-E.pdf
// Table 3
static const float3x3 lin_rgb_to_xyz_matrix =
{
	0.708, 0.292, 0.000,
	0.170, 0.797, 0.033,
	0.131, 0.046, 0.823
};

float3 linear_rgb_to_xyz(float3 lin_rgb) {
	return mul(lin_rgb_to_xyz_matrix, lin_rgb);
}

// From https://en.wikipedia.org/wiki/CIELAB_color_space
float cielab_f(float t) {
	if (t > 0.008856) {
		return pow(t, 0.3333);
	}
	else {
		return (7.787 * t) + (4.0 / 29.0);
	}
}

static const float3 xyz_reference_white = float3(0.95048, 1.00, 1.088840);
float xyz_to_cielab(float3 xyz) {
	float f_x = cielab_f(xyz.x / xyz_reference_white.x);
	float f_y = cielab_f(xyz.y / xyz_reference_white.y);
	float f_z = cielab_f(xyz.z / xyz_reference_white.z);

	float L = 116 * f_y - 16;
	float a = 500 * (f_x - f_y);
	float b = 200 * (f_y - f_z);

	return float3(L, a, b);
}