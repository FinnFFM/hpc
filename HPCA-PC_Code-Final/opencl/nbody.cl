

__kernel void compute_forces(
    __global const float* x,
    __global const float* y,
    __global const float* m,
    __global float* ax,
    __global float* ay,
    const unsigned int N,
    const float G,
    const float soft
)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    float axi = 0.0f;
    float ayi = 0.0f;

    const float xi = x[i];
    const float yi = y[i];

    for (int j = 0; j < N; ++j) {

        if (i == j) continue;

        float dx = x[j] - xi;
        float dy = y[j] - yi;

        float dist2 = dx*dx + dy*dy + soft*soft;

        // OpenCL hat native rsqrt
        float invDist = rsqrt(dist2);
        float invDist3 = invDist * invDist * invDist;

        float f = G * m[j] * invDist3;

        axi += dx * f;
        ayi += dy * f;
    }

    ax[i] = axi;
    ay[i] = ayi;
}


__kernel void kick_drift(
    __global float* x,
    __global float* y,
    __global float* vx,
    __global float* vy,
    __global const float* ax,
    __global const float* ay,
    const unsigned int N,
    const float dt
)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    const float half_dt = 0.5f * dt;

    // kick
    vx[i] += ax[i] * half_dt;
    vy[i] += ay[i] * half_dt;

    // drift
    x[i] += vx[i] * dt;
    y[i] += vy[i] * dt;
}

__kernel void kick(
    __global float* vx,
    __global float* vy,
    __global const float* ax,
    __global const float* ay,
    const unsigned int N,
    const float dt
)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    const float half_dt = 0.5f * dt;

    vx[i] += ax[i] * half_dt;
    vy[i] += ay[i] * half_dt;
}