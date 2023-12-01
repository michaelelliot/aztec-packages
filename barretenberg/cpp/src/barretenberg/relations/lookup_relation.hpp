#pragma once
#include "barretenberg/relations/relation_types.hpp"

namespace proof_system {

/**
 * @brief LookupRelationImpl defines the algebra for the lookup polynomial:
 *
 *                       ∏ (1 + β) ⋅ (q_lookup*f_k + γ) ⋅ (t_k + βt_{k+1} + γ(1 + β))
 *  Z_lookup(g^j) = --------------------------------------------------------------------------
 *                                      ∏ (s_k + βs_{k+1} + γ(1 + β))
 *
 *
 * The method `compute_numerator_term` computes polynomials f, t and incorporate them into terms that are ultimately
 * needed to construct the grand product polynomial Z_lookup(X): Note 1: In the above, 't' is associated with table
 * values (and is not to be confused with the quotient polynomial, also refered to as 't' elsewhere). Polynomial 's' is
 * the sorted  concatenation of the witnesses and the table values.
 *
 * @tparam FF parametrises the prime field class being used
 */
template <typename FF_> class LookupRelationImpl {
  public:
    using FF = FF_;

    static constexpr std::array<size_t, 2> SUBRELATION_PARTIAL_LENGTHS{
        6, // grand product construction sub-relation
        3  // left-shiftable polynomial sub-relation
    };

    static constexpr std::array<size_t, 2> TOTAL_LENGTH_ADJUSTMENTS{
        6, // grand product construction sub-relation
        0  // left-shiftable polynomial sub-relation
    };
    /**
     * @brief Get the grand product polynomial object (either from the proving key or AllEntities depending on context)
     *
     * @param input
     * @return auto& either std::span<FF> or Flavor::Polynomial depending on context
     */
    inline static auto& get_grand_product_polynomial(auto& input) { return input.z_lookup; }

    /**
     * @brief Get the shifted grand product polynomial object (either from the proving key or AllEntities depending on
     * context)
     *
     * @param input
     * @return auto& either std::span<FF> or Flavor::Polynomial depending on context
     */
    inline static auto& get_shifted_grand_product_polynomial(auto& input) { return input.z_lookup_shift; }

    /**
     * @brief Compute numerator term of the lookup relation:
     *
     *     N_{index} = (1 + β) ⋅ ∏ (q_lookup*f_k + γ) ⋅ (t_k + βt_{k+1} + γ(1 + β))
     *
     * @tparam AccumulatorTypes
     * @param in
     * @param relation_parameters
     * @param index If calling this method over vector inputs, index >= 0
     */
    template <typename Accumulator, typename AllEntities, typename Parameters>
    inline static Accumulator compute_grand_product_numerator(const AllEntities& in, const Parameters& params)
    {
        using View = typename Accumulator::View;
        using ParameterView = GetParameterView<Parameters, View>;

        const auto& beta = ParameterView(params.beta);
        const auto& gamma = ParameterView(params.gamma);
        const auto& eta = ParameterView(params.eta);
        const auto eta_sqr = eta * eta;
        const auto eta_cube = eta_sqr * eta;

        const auto one_plus_beta = beta + FF(1);
        const auto gamma_by_one_plus_beta = gamma * one_plus_beta;

        const Accumulator& w_1 = in.w_l;
        const Accumulator& w_2 = in.w_r;
        const Accumulator& w_3 = in.w_o;

        const Accumulator& w_1_shift = in.w_l_shift;
        const Accumulator& w_2_shift = in.w_r_shift;
        const Accumulator& w_3_shift = in.w_o_shift;

        const Accumulator& table_1 = in.table_1;
        const Accumulator& table_2 = in.table_2;
        const Accumulator& table_3 = in.table_3;
        const Accumulator& table_4 = in.table_4;

        const Accumulator& table_1_shift = in.table_1_shift;
        const Accumulator& table_2_shift = in.table_2_shift;
        const Accumulator& table_3_shift = in.table_3_shift;
        const Accumulator& table_4_shift = in.table_4_shift;

        const Accumulator& table_index = in.q_o;
        const Accumulator& column_1_step_size = in.q_r;
        const Accumulator& column_2_step_size = in.q_m;
        const Accumulator& column_3_step_size = in.q_c;
        const Accumulator& q_lookup = in.q_lookup;

        // (w_1 + q_2*w_1_shift) + η(w_2 + q_m*w_2_shift) + η²(w_3 + q_c*w_3_shift) + η³q_index.
        // deg 2 or 4
        auto wire_accum = (w_1 + column_1_step_size * w_1_shift) + (w_2 + column_2_step_size * w_2_shift) * eta +
                          (w_3 + column_3_step_size * w_3_shift) * eta_sqr + table_index * eta_cube;

        // t_1 + ηt_2 + η²t_3 + η³t_4
        // deg 1 or 4
        auto table_accum = table_1 + table_2 * eta + table_3 * eta_sqr + table_4 * eta_cube;

        // t_1_shift + ηt_2_shift + η²t_3_shift + η³t_4_shift
        // deg 4
        auto table_accum_shift =
            table_1_shift + table_2_shift * eta + table_3_shift * eta_sqr + table_4_shift * eta_cube;

        auto tmp = (q_lookup * wire_accum + gamma);                               // deg 2 or 4
        tmp *= (table_accum + table_accum_shift * beta + gamma_by_one_plus_beta); // 1 or 5
        tmp *= one_plus_beta;                                                     // deg 1
        return tmp;                                                               // deg 4 or 10
    }

    /**
     * @brief Compute denominator term of the lookup relation:
     *
     *      (s_k + βs_{k+1} + γ(1 + β))
     *
     * @tparam AccumulatorTypes
     * @param in
     * @param relation_parameters
     * @param index
     */
    template <typename Accumulator, typename AllEntities, typename Parameters>
    inline static Accumulator compute_grand_product_denominator(const AllEntities& in, const Parameters& params)
    {

        using View = typename Accumulator::View;
        using ParameterView = GetParameterView<Parameters, View>;

        const auto& beta = ParameterView(params.beta);
        const auto& gamma = ParameterView(params.gamma);

        const auto one_plus_beta = beta + FF(1);
        const auto gamma_by_one_plus_beta = gamma * one_plus_beta; // deg 0 or 2

        // Contribution (1)
        const Accumulator& s_accum = in.sorted_accum;
        const Accumulator& s_accum_shift = in.sorted_accum_shift;

        auto tmp = (s_accum + s_accum_shift * beta + gamma_by_one_plus_beta); // 1 or 2
        return tmp;
    }

    /**
     * @brief Compute contribution of the lookup grand prod relation for a given edge (internal function)
     *
     * @details This the relation confirms faithful calculation of the lookup grand
     * product polynomial Z_lookup. The contribution is
     *      z_lookup * (1 + β) * [q_lookup * f + γ] * (t_accum_k + βt_accum_{k+1} + γ(1 + β)) -
     *      z_lookup_shift * (s_accum_k + βs_accum_{k+1} + γ(1 + β))
     * where
     *      f = (w_1 + q_2*w_1_shift) + η(w_2 + q_m*w_2_shift) + η²(w_3 + q_c*w_3_shift) + η³q_index,
     *      t_accum = table_1 + ηtable_2 + η²table_3 + η³table_4, and
     *      s_accum = s_1 + ηs_2 + η²s_3 + η³s_4.
     * Note: Selectors q_2, q_m and q_c are repurposed as 'column step size' for lookup gates.
     *
     * @param evals transformed to `evals + C(in(X)...)*scaling_factor`
     * @param in an std::array containing the fully extended Univariate edges.
     * @param parameters contains beta, gamma, and public_input_delta, ....
     * @param scaling_factor optional term to scale the evaluation before adding to evals.
     */
    template <typename ContainerOverSubrelations, typename AllEntities, typename Parameters>
    inline static void accumulate(ContainerOverSubrelations& accumulators,
                                  const AllEntities& in,
                                  const Parameters& params,
                                  const FF& scaling_factor)
    {

        {
            using Accumulator = std::tuple_element_t<0, ContainerOverSubrelations>;
            using View = typename Accumulator::View;
            using ParameterView = GetParameterView<Parameters, View>;

            const auto& grand_product_delta = ParameterView(params.lookup_grand_product_delta);

            const Accumulator& z_lookup = in.z_lookup;
            const Accumulator& z_lookup_shift = in.z_lookup_shift;

            const Accumulator& lagrange_first = in.lagrange_first;
            const Accumulator& lagrange_last = in.lagrange_last;

            const auto lhs = compute_grand_product_numerator<Accumulator>(in, params);   // deg 4 or 10
            const auto rhs = compute_grand_product_denominator<Accumulator>(in, params); // deg 1 or 2

            // (deg 5 or 11) - (deg 3 or 5)
            const Accumulator tmp =
                lhs * (z_lookup + lagrange_first) - rhs * (z_lookup_shift + lagrange_last * grand_product_delta);
            std::get<0>(accumulators) += tmp * scaling_factor;
        };

        {
            using Accumulator = std::tuple_element_t<1, ContainerOverSubrelations>;
            // using View = typename Accumulator::View;
            // auto z_lookup_shift = View(in.z_lookup_shift);
            // auto lagrange_last = View(in.lagrange_last);

            // Contribution (2)
            Accumulator tmp(in.lagrange_last);
            tmp *= in.z_lookup_shift;
            tmp *= scaling_factor;
            std::get<1>(accumulators) += tmp;
        };
    };
};

template <typename FF> using LookupRelation = Relation<LookupRelationImpl<FF>>;

} // namespace proof_system