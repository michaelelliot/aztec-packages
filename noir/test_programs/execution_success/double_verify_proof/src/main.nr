use dep::std;

fn main(
    verification_key: [Field; 114],
    // This is the proof without public inputs attached.
    // 
    // This means: the size of this does not change with the number of public inputs.
    proof: [Field; 93],
    public_inputs: [Field; 1],
    // This is currently not public. It is fine given that the vk is a part of the circuit definition.
    // I believe we want to eventually make it public too though.
    key_hash: Field,
    proof_b: [Field; 93]
) {
    // Create dummy aggregation object
    // It is no longer needed but we don't want to make a breaking change to the serialization format
    // just yet. The object can be anything as the backend is no longer checking it anyways.
    let unused_input_aggregation_object = [verification_key[0]];

    let _ = std::verify_proof(
        verification_key.as_slice(),
        proof.as_slice(),
        public_inputs.as_slice(),
        key_hash,
        unused_input_aggregation_object
    );

    let _ = std::verify_proof(
        verification_key.as_slice(),
        proof_b.as_slice(),
        public_inputs.as_slice(),
        key_hash,
        unused_input_aggregation_object
    );
}
