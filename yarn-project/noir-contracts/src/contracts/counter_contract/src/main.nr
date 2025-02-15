contract Counter {
    // docs:start:imports
    use dep::protocol_types::address::AztecAddress;
    use dep::aztec::{
        context::{PrivateContext, Context},
        note::{
            note_header::NoteHeader,
            utils as note_utils,
        },
        state_vars::map::Map,
    };
    use dep::value_note::{
            balance_utils,
            value_note::{
                ValueNoteMethods,
                VALUE_NOTE_LEN,
            },
    };
    use dep::easy_private_state::easy_private_state::EasyPrivateUint;
    // docs:end:imports

    // docs:start:storage_struct

    struct Storage {
        counters: Map<EasyPrivateUint>,
    }

    // docs:end:storage_struct

    // docs:start:storage_init
    impl Storage {
        fn init(context: Context) -> Self {
            Storage {
                counters: Map::new(
                    context,
                    1,
                    |context, slot| {
                        EasyPrivateUint::new(context, slot)
                    },
                ),
            }
        }
    }
    // docs:end:storage_init

    // docs:start:constructor
    #[aztec(private)]
    fn constructor(headstart: u120, owner: AztecAddress) {
        let counters = storage.counters;
        counters.at(owner.to_field()).add(headstart, owner);
    }
    // docs:end:constructor

    // docs:start:increment
    #[aztec(private)]
      fn increment(owner: AztecAddress) {
        let counters = storage.counters;
        counters.at(owner.to_field()).add(1, owner);
    }
    // docs:end:increment

    // docs:start:get_counter
    unconstrained fn get_counter(owner: AztecAddress) -> pub Field {
        let counters = storage.counters;
        balance_utils::get_balance(counters.at(owner.to_field()).set)
    }
    // docs:end:get_counter

    // docs:start:nullifier
    unconstrained fn compute_note_hash_and_nullifier(
        contract_address: Field,
        nonce: Field,
        storage_slot: Field,
        serialized_note: [Field; VALUE_NOTE_LEN]
    ) -> pub [Field; 4] {
        let _address = AztecAddress::from_field(contract_address); // TODO(benesjan) https://github.com/AztecProtocol/aztec-packages/issues/3669
        let note_header = NoteHeader::new(_address, nonce, storage_slot);
        note_utils::compute_note_hash_and_nullifier(ValueNoteMethods, note_header, serialized_note)
    }
    // docs:end:nullifier
}
