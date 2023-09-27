#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/string.hpp>

using namespace eosio;

class [[eosio::contract("nice1swapone")]] nice1swapone : public contract {

public:
  nice1swapone(name receiver, name code, datastream<const char*> ds) : contract(receiver, code, ds) {}

  //The "asetswap" action allows you to add lines to the table that stores the data necessary to process the swaps between tokens.
  [[eosio::action]]
  void asetswap(
    name ref,
    name receiving_token_contract_account,
    symbol receiving_tick,
    asset receiving_qty,
    name sending_token_contract_account,
    symbol sending_tick,
    asset sending_qty,
    uint64_t memo_expected,
    bool active = false
  ) 
  {
      name owner = get_first_receiver();
  
      require_auth(owner);
      check(receiving_qty.is_valid(), "Invalid receiving quantity");
      check(sending_qty.is_valid(), "Invalid sending quantity");
  
      // Check if "ref" already exists in the table
      testa1_table testa1(get_self(), get_self().value);
      auto existing_row = testa1.find(ref.value);
      
      if (existing_row != testa1.end()) {
        // If "ref" already exists, deny transaction
        check(false, "Ref already exists in the table");
      }

      // Check if "memo_expected" already exists in the table as a secondary index
      auto memo_index = testa1.get_index<"memoexpected"_n>();
      auto memo_existing_row = memo_index.find(memo_expected);
      
      if (memo_existing_row != memo_index.end()) {
        // If "memo_expected" already exists, deny the transaction
        check(false, "Memo_expected already in use");
      }

      // If "ref" and "memo_expected" are valid, create a new entry in the table
      testa1.emplace(owner, [&](auto& row) {
        row.ref = ref;
        row.receiving_token_contract_account = receiving_token_contract_account;
        row.receiving_tick = receiving_tick;
        row.receiving_qty = receiving_qty;
        row.sending_token_contract_account = sending_token_contract_account;
        row.sending_tick = sending_tick;
        row.sending_qty = sending_qty;
        row.memo_expected = memo_expected;
        row.active = active;
      });
  }

  //The "dswap" action allows deleting entries from the table.
  [[eosio::action]]
  void dswap(name owner, name ref) 
  {
    require_auth(owner);

    // Check if "ref" exists in the table
    testa1_table testa1(get_self(), get_self().value);
    auto existing_row = testa1.find(ref.value);
    
    if (existing_row != testa1.end()) {
      // If "ref" exists, delete the entry
      testa1.erase(existing_row);
    } else {
      // If "ref" does not exist, display an error message
      check(false, "Ref does not exist in the table");
    }

  }

  //The "bypass" action allows to change the active state of an entry from "0" to "1" and vice versa.
  [[eosio::action]]
  void bypass(name owner, name ref, bool new_active_state) 
  {
    require_auth(owner);

    // Check if "ref" exists in the table
    testa1_table testa1(get_self(), get_self().value);
    auto existing_row = testa1.find(ref.value);
    
    if (existing_row == testa1.end()) {
      // If "ref" does not exist, cancel the transaction and display an error.
      check(false, "Ref does not exist in the table");
    }

    // Verify if the new status is different from the current status in the table
    if (existing_row->active == new_active_state) {
      // If the status is the same, deny transaction
      check(false, "Selected state is already set");
    }

    // Update "active" status in the table
    testa1.modify(existing_row, owner, [&](auto& row) {
      row.active = new_active_state;
    });
  }

  //The following action is in charge of listening for incoming transfers in the account and swapping tokens based on the information in the table
  [[eosio::on_notify("*::transfer")]]
  void on_transfer(name from, name to, asset quantity, std::string memo) 
  {
    if (to == get_self()) {
      // Check if the field "memo" exactly matches "memo_expected" in table.
      testa1_table testa1(get_self(), get_self().value);
      auto memo_index = testa1.get_index<"memoexpected"_n>();
      auto memo_existing_row = memo_index.find(std::stoull(memo));

      if (memo_existing_row != memo_index.end() && std::to_string(memo_existing_row->memo_expected) == memo) 
      {
        // Check if "active" is equal to 1 in the table entry
        check(memo_existing_row->active == 1, "No available");

         // Get "receiving_token_contract_account" from matching table entry
        name expected_contract = memo_existing_row->receiving_token_contract_account;
        // Get "sending_token_contract_account" from matching table entry
        name destination_contract = memo_existing_row->sending_token_contract_account;

        // Verify if the tokens received in the transaction come from the expected contract.
        name source_contract = get_first_receiver(); 

        check(source_contract == expected_contract, "Tokens must come from the expected contract");

        // Check if the number of tokens transferred matches "receiving_qty" in the table entry
        check(quantity == memo_existing_row->receiving_qty, "Invalid quantity");

        // Get the token and the number of tokens to send from the matched entry
        symbol sending_symbol = memo_existing_row->sending_tick;
        asset sending_qty = memo_existing_row->sending_qty;

        // Verify that the amount to be sent is valid
        check(sending_qty.is_valid(), "Invalid sending quantity");

        // Defines the number of tokens to be transferred
        asset quantity_to_transfer = sending_qty;

        // Create an instance of 'transfer' action on "sending_token_contract_account" contract with the data received from the matching entry.
        action transfer_action = action(
            permission_level{get_self(), "active"_n},
             destination_contract,
            "transfer"_n,
            std::make_tuple(get_self(), from, quantity_to_transfer, std::string("ALL OK"))
        );

        // Send the 'transfer' action.
        transfer_action.send();
      } else {
        // Displays "memo_expected" error not matched in incoming transaction
        check(false, "Memo does not match any expected memo");
      }
    }
  };

  

private:
  struct [[eosio::table]] test_row {
    name ref;
    name receiving_token_contract_account;
    symbol receiving_tick;
    asset receiving_qty;
    name sending_token_contract_account;
    symbol sending_tick;
    asset sending_qty;
    uint64_t memo_expected;
    bool active;

    uint64_t primary_key() const { return ref.value; }
    uint64_t by_memo_expected() const { return memo_expected; }
  };

  typedef multi_index<"testa1"_n, test_row,
    indexed_by<"memoexpected"_n, const_mem_fun<test_row, uint64_t, &test_row::by_memo_expected>>
  > testa1_table;

  uint128_t combine_ids(
    uint64_t contract1, uint64_t symbol1, uint64_t precision1,
    uint64_t contract2, uint64_t symbol2, uint64_t precision2
  ) {
    return (
      (static_cast<uint128_t>(contract1) << 96) |
      (static_cast<uint128_t>(symbol1) << 64) |
      (static_cast<uint128_t>(precision1) << 56) |
      (static_cast<uint128_t>(contract2) << 40) |
      (static_cast<uint128_t>(symbol2) << 16) |
      (static_cast<uint128_t>(precision2))
    );
  }
};
