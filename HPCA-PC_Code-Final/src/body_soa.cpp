#include "body_soa.h"
#include <algorithm>

#include <experimental/simd>
namespace stdx=std::experimental;
using float_v = stdx::native_simd<float>;
constexpr size_t VEC_SIZE = float_v::size();

BodySoA::BodySoA(size_t n_, size_t n_padded_) : n(n_) {

        // Addiere (BATCH_SIZE - 1), um bei der Ganzzahldivision aufzurunden.
        size_t num_batches = (n + VEC_SIZE - 1) / VEC_SIZE;
        // Die Gesamtzahl der Plätze berechnen
        n_padded = num_batches * VEC_SIZE;


    this->n_padded = n_padded;

    // Initialisiere alle Vektoren.
    // .assign(Größe, Wert) ändert die Größe und füllt alles mit dem Wert.
    // Die Dummy Körper am Ende haben Masse 0.0f,
    // damit sie keine Gravitation auf andere ausüben
    x.assign(n_padded, 0.0f);
    y.assign(n_padded, 0.0f);
    vx.assign(n_padded, 0.0f);
    vy.assign(n_padded, 0.0f);
    ax.assign(n_padded, 0.0f);
    ay.assign(n_padded, 0.0f);
    m.assign(n_padded, 0.0f);
}