#include "barretenberg/eccvm/eccvm_composer.hpp"
#include "barretenberg/goblin/goblin.hpp"
#include "barretenberg/goblin/translation_evaluations.hpp"
#include "barretenberg/proof_system/circuit_builder/eccvm/eccvm_circuit_builder.hpp"
#include "barretenberg/proof_system/circuit_builder/goblin_ultra_circuit_builder.hpp"
#include "barretenberg/proof_system/circuit_builder/ultra_circuit_builder.hpp"
#include "barretenberg/translator_vm/goblin_translator_composer.hpp"
#include "barretenberg/ultra_honk/ultra_composer.hpp"

#include <gtest/gtest.h>

#pragma GCC diagnostic ignored "-Wunused-variable"

using namespace proof_system::honk;

namespace test_full_goblin_composer {

class FullGoblinComposerTests : public ::testing::Test {
  protected:
    static void SetUpTestSuite()
    {
        barretenberg::srs::init_crs_factory("../srs_db/ignition");
        barretenberg::srs::init_grumpkin_crs_factory("../srs_db/grumpkin");
    }

    using Curve = curve::BN254;
    using FF = Curve::ScalarField;
    using Fbase = Curve::BaseField;
    using Point = Curve::AffineElement;
    using CommitmentKey = pcs::CommitmentKey<Curve>;
    using OpQueue = proof_system::ECCOpQueue;
    using GoblinUltraBuilder = proof_system::GoblinUltraCircuitBuilder;
    using ECCVMFlavor = flavor::ECCVM;
    using ECCVMBuilder = proof_system::ECCVMCircuitBuilder<ECCVMFlavor>;
    using ECCVMComposer = ECCVMComposer_<ECCVMFlavor>;
    using TranslatorFlavor = flavor::GoblinTranslator;
    using TranslatorBuilder = proof_system::GoblinTranslatorCircuitBuilder;
    using TranslatorComposer = GoblinTranslatorComposer;
    using TranslatorConsistencyData = barretenberg::TranslationEvaluations;
    using Proof = proof_system::plonk::proof;
    using NativeVerificationKey = flavor::GoblinUltra::VerificationKey;

    struct VerifierInput {
        Proof proof;
        std::shared_ptr<NativeVerificationKey> verification_key;
    };

    static constexpr size_t NUM_OP_QUEUE_COLUMNS = flavor::GoblinUltra::NUM_WIRES;

    /**
     * @brief Generate a simple test circuit with some ECC op gates and conventional arithmetic gates
     *
     * @param builder
     */
    static void generate_test_circuit(GoblinUltraBuilder& builder, [[maybe_unused]] const Proof& previous_proof = {})
    {
        // Add some arbitrary ecc op gates
        for (size_t i = 0; i < 3; ++i) {
            auto point = Point::random_element();
            auto scalar = FF::random_element();
            builder.queue_ecc_add_accum(point);
            builder.queue_ecc_mul_accum(point, scalar);
        }
        // queues the result of the preceding ECC
        builder.queue_ecc_eq(); // should be eq and reset

        // Add some conventional gates that utilize public inputs
        for (size_t i = 0; i < 10; ++i) {
            FF a = FF::random_element();
            FF b = FF::random_element();
            FF c = FF::random_element();
            FF d = a + b + c;
            uint32_t a_idx = builder.add_public_variable(a);
            uint32_t b_idx = builder.add_variable(b);
            uint32_t c_idx = builder.add_variable(c);
            uint32_t d_idx = builder.add_variable(d);

            builder.create_big_add_gate({ a_idx, b_idx, c_idx, d_idx, FF(1), FF(1), FF(1), FF(-1), FF(0) });
        }
    }

    /**
     * @brief Mock the interactions of a simple curcuit with the op_queue
     * @details The transcript aggregation protocol in the Goblin proof system can not yet support an empty "previous
     * transcript" (see issue #723). This function mocks the interactions with the op queue of a fictional "first"
     * circuit. This way, when we go to generate a proof over our first "real" circuit, the transcript aggregation
     * protocol can proceed nominally. The mock data is valid in the sense that it can be processed by all stages of
     * Goblin as if it came from a genuine circuit.
     *
     * @todo WOKTODO: this is a zero commitments issue
     *
     * @param op_queue
     */
    static void perform_op_queue_interactions_for_mock_first_circuit(
        std::shared_ptr<proof_system::ECCOpQueue>& op_queue)
    {
        proof_system::GoblinUltraCircuitBuilder builder{ op_queue };

        // Add a mul accum op and an equality op
        auto point = Point::one() * FF::random_element();
        auto scalar = FF::random_element();
        builder.queue_ecc_mul_accum(point, scalar);
        builder.queue_ecc_eq();

        op_queue->set_size_data();

        // Manually compute the op queue transcript commitments (which would normally be done by the prover)
        auto crs_factory_ = barretenberg::srs::get_crs_factory();
        auto commitment_key = CommitmentKey(op_queue->get_current_size(), crs_factory_);
        std::array<Point, NUM_OP_QUEUE_COLUMNS> op_queue_commitments;
        size_t idx = 0;
        for (auto& entry : op_queue->get_aggregate_transcript()) {
            op_queue_commitments[idx++] = commitment_key.commit(entry);
        }
        // Store the commitment data for use by the prover of the next circuit
        op_queue->set_commitment_data(op_queue_commitments);
    }
};

/**
 * @brief Test proof construction/verification for a circuit with ECC op gates, public inputs, and basic arithmetic
 * gates
 * @note We simulate op queue interactions with a previous circuit so the actual circuit under test utilizes an op queue
 * with non-empty 'previous' data. This avoids complications with zero-commitments etc.
 *
 */
TEST_F(FullGoblinComposerTests, SimpleCircuit)
{
    auto op_queue = std::make_shared<proof_system::ECCOpQueue>();

    // Add mock data to op queue to simulate interaction with a "first" circuit
    perform_op_queue_interactions_for_mock_first_circuit(op_queue);

    proof_system::plonk::proof previous_proof;

    // Construct a series of simple Goblin circuits; generate and verify their proofs
    size_t NUM_CIRCUITS = 4;
    for (size_t circuit_idx = 0; circuit_idx < NUM_CIRCUITS; ++circuit_idx) {
        proof_system::GoblinUltraCircuitBuilder builder{ op_queue };

        generate_test_circuit(builder, previous_proof);

        // The same composer is used to manage Honk and Merge prover/verifier
        proof_system::honk::GoblinUltraComposer composer;

        // Construct and verify Ultra Goblin Honk proof
        auto instance = composer.create_instance(builder);
        auto prover = composer.create_prover(instance);
        auto verifier = composer.create_verifier(instance);
        auto honk_proof = prover.construct_proof();
        bool honk_verified = verifier.verify_proof(honk_proof);
        EXPECT_TRUE(honk_verified);

        // Construct and verify op queue merge proof
        auto merge_prover = composer.create_merge_prover(op_queue);
        auto merge_verifier = composer.create_merge_verifier(/*srs_size=*/10); // WORKTODO set this
        auto merge_proof = merge_prover.construct_proof();
        bool merge_verified = merge_verifier.verify_proof(merge_proof);
        EXPECT_TRUE(merge_verified);
    }

    // Execute the ECCVM
    // TODO(https://github.com/AztecProtocol/barretenberg/issues/785) Properly initialize transcript
    auto eccvm_builder = ECCVMBuilder(op_queue);
    auto eccvm_composer = ECCVMComposer();
    auto eccvm_prover = eccvm_composer.create_prover(eccvm_builder);
    auto eccvm_verifier = eccvm_composer.create_verifier(eccvm_builder);
    auto eccvm_proof = eccvm_prover.construct_proof();
    bool eccvm_verified = eccvm_verifier.verify_proof(eccvm_proof);
    EXPECT_TRUE(eccvm_verified);

    // Execute the Translator
    // TODO(https://github.com/AztecProtocol/barretenberg/issues/786) Properly derive batching_challenge
    auto batching_challenge = Fbase::random_element();
    auto evaluation_input = eccvm_prover.evaluation_challenge_x;
    proof_system::GoblinTranslatorCircuitBuilder translator_builder{ batching_challenge, evaluation_input, op_queue };
    GoblinTranslatorComposer translator_composer;
    GoblinTranslatorProver translator_prover = translator_composer.create_prover(translator_builder);
    GoblinTranslatorVerifier translator_verifier = translator_composer.create_verifier(translator_builder);
    proof_system::plonk::proof translator_proof = translator_prover.construct_proof();
    bool accumulator_construction_verified = translator_verifier.verify_proof(translator_proof);
    bool translation_verified = translator_verifier.verify_translation(eccvm_prover.translation_evaluations);
    EXPECT_TRUE(accumulator_construction_verified && translation_verified);
}

/**
 * @brief A full Goblin test that mimicks the basic aztec client architecture
 *
 */
TEST_F(FullGoblinComposerTests, Pseudo)
{
    barretenberg::Goblin goblin;

    // WORKTODO: In theory we could use the ops from the first circuit instead of these fake ops but then we'd still
    // have to manually compute and call set_commitments since we can call prove with no prior data.
    perform_op_queue_interactions_for_mock_first_circuit(goblin.op_queue);

    // Construct an initial goblin ultra circuit
    GoblinUltraBuilder initial_circuit_builder{ goblin.op_queue };
    generate_test_circuit(initial_circuit_builder);

    // Construct a proof of the initial circuit to be recursively verified
    auto composer = GoblinUltraComposer();
    auto instance = composer.create_instance(initial_circuit_builder);
    auto prover = composer.create_prover(instance);
    auto proof = prover.construct_proof();
    auto verification_key = instance->compute_verification_key();
    VerifierInput verifier_input = { proof, verification_key };
    { // Natively verify for testing purposes only
        auto verifier = composer.create_verifier(instance);
        bool honk_verified = verifier.verify_proof(proof);
        EXPECT_TRUE(honk_verified);
    }

    // Construct a merge proof to be recursively verified
    auto merge_prover = composer.create_merge_prover(goblin.op_queue);
    auto merge_proof = merge_prover.construct_proof();
    { // Natively verify for testing purposes only
        auto merge_verifier = composer.create_merge_verifier(/*srs_size=*/10); // WORKTODO set this
        bool merge_verified = merge_verifier.verify_proof(merge_proof);
        EXPECT_TRUE(merge_verified);
    }

    // const auto folding_verifier = [](GoblinUltraBuilder& builder) { generate_test_circuit(builder); };

    // WORKTODO: possible construct the proof of a "first" circuit here. then the first kernel has something to
    // recursively verify. This resembles the actual aztec architecture which defines an initial_kernel as something
    // distinct from the inner_kernel

    // Construct a series of simple Goblin circuits; generate and verify their proofs
    size_t NUM_CIRCUITS = 4;
    for (size_t circuit_idx = 0; circuit_idx < NUM_CIRCUITS; ++circuit_idx) {
        GoblinUltraBuilder circuit_builder{ goblin.op_queue };

        // Construct a circuit with logic resembling that of the "kernel circuit"
        {
            // generic operations e.g. state updates (just arith gates for now)
            generate_test_circuit(circuit_builder);

            // execute recursive aggregation of previous kernel
            // auto verification_key = std::make_shared<VerificationKey>(&circuit_builder, native_verification_key);
            // RecursiveVerifier verifier(&circuit_builder, verification_key);
            // auto pairing_points = verifier.verify_proof(prev_kernel_proof);

            // execute recursive aggregation of app circuit (ignore this for now)

            // Possibly use lambda syntax like this: folding_verifier(/* goblin. */ circuit_builder); // WORKTODO
        }
        // Complete kernel circuit logic by with recursive verification of merge proof then construct proof/instance
        goblin.accumulate(circuit_builder); // merge prover done in here
    }

    // auto vms_verified = goblin.prove();
    // bool verified = goblin.verified && vms_verified;
    // // bool verified = goblin.verify(proof)
    // EXPECT_TRUE(verified);
}

// TODO(https://github.com/AztecProtocol/barretenberg/issues/787) Expand these tests.
} // namespace test_full_goblin_composer
