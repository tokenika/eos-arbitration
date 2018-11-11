#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/time.hpp>

using namespace eosio;
using namespace std;

time_point current_time_point();

class [[eosio::contract("dep")]] dep : public contract {
  public:
      using contract::contract;
    
      static constexpr uint32_t refund_delay_sec = 30;


      [[eosio::action]]
      void hi( name user ) {
          require_auth(user);
          print( "Hello, ", name{user});
      }

      [[eosio::action]]
      void transfer(name from, name to, asset quantity, string memo);

      [[eosio::action]]
      void openaccount(name account);

      [[eosio::action]]
      void withdraw( name account, asset amount);

      [[eosio::action]]
      void reclaim(name account);

      static asset get_balance( name token_contract_account, name owner, symbol_code sym_code ) {
          accounts accountstable( token_contract_account, owner.value );
          const auto& ac = accountstable.get( sym_code.raw() );
          return ac.balance;
      }
  private:
      struct [[eosio::table]] account {
          asset    balance;

          uint64_t primary_key()const { return balance.symbol.code().raw(); }
      };

      struct [[eosio::table]] withdrawal_request {
          name            owner;
          time_point_sec  request_time;
          asset amount; 
          uint64_t primary_key()const { return amount.symbol.code().raw(); }
      };

      typedef eosio::multi_index< "withdrawals"_n, withdrawal_request > withdrawals;
      typedef eosio::multi_index< "accounts"_n, account > accounts;
      void sub_balance( name owner, asset value );
      void add_balance( name owner, asset value, name ram_payer );
};

void dep::transfer( name from, name to, asset quantity, string memo ) {
    if (from == _self || to != _self) {
        return;
    }
    eosio_assert(quantity.symbol == symbol("SYS", 4), "I think you're looking for another contract");
    eosio_assert(quantity.is_valid(), "Are you trying to corrupt me?");
    eosio_assert(quantity.amount > 0, "When pigs fly");
    accounts to_acnts( _self, from.value );
    auto to_acnt = to_acnts.find( quantity.symbol.code().raw() );
    eosio_assert( to_acnt != to_acnts.end(), "Don't send us your money before opening account" );
    add_balance(from, quantity, from);
}

void dep::openaccount( name account) {
    require_auth(account);
    accounts to_acnts( _self, account.value );
    auto to_acnt = to_acnts.find( symbol("SYS", 4).code().raw() );
    eosio_assert( to_acnt == to_acnts.end(), "Account already exists" );
    add_balance(account, asset(0, symbol("SYS", 4)), account);
}

void dep::withdraw(name account, asset amount) {
    require_auth(account);
    accounts from_acnts( _self, account.value );
    sub_balance(account, amount);

    withdrawals requests(_self, account.value);
    auto request = requests.find(symbol("SYS", 4).code().raw());
    if (request == requests.end()) {
        requests.emplace(account, [&](auto &req) {
            req.owner = account;
            req.request_time = current_time_point();
            req.amount = amount;
        });
    } else {
        requests.modify(request, account, [&](auto &req) {
            req.amount += amount;
            req.request_time = current_time_point();
        });
    }
}

void dep::reclaim( name account) {
    require_auth(account);
    withdrawals requests(_self, account.value);
    auto request = requests.find(symbol("SYS", 4).code().raw());
    eosio_assert(request != requests.end(), "No request"); 
    eosio_assert( request->request_time + seconds(refund_delay_sec) <= current_time_point(),
            "refund is not available yet" );
    action reclaim = action(
        permission_level{get_self() ,"active"_n},
        "eosio.token"_n,
        "transfer"_n,
        std::make_tuple(get_self(), account, request->amount, std::string("Here are your tokens"))
    );
    reclaim.send();
    requests.erase(request);
}

void dep::sub_balance( name owner, asset value ) {
   accounts from_acnts( _self, owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no account found" );
   eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
       a.balance -= value;
   });
}

void dep::add_balance( name owner, asset value, name ram_payer )
{
   accounts to_acnts( _self, owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        auto self = receiver;
        if(code == self && action == name("hi").value) {
              execute_action(name(receiver), name(code), &dep::hi); 
        } else if(code == self && action == name("openaccount").value) {
              execute_action(name(receiver), name(code), &dep::openaccount); 
        } else if(code == self && action == name("withdraw").value) {
              execute_action(name(receiver), name(code), &dep::withdraw); 
        } else if(code == self && action == name("reclaim").value) {
              execute_action(name(receiver), name(code), &dep::reclaim); 
        } else if(code == name("eosio.token").value && action == name("transfer").value) {
              execute_action(name(receiver), name(code), &dep::transfer); 
        } else{
            eosio_assert(false, (string("Ooops - action not configured: ")+ name(action).to_string()).c_str());
        }
    }
}

time_point current_time_point() {
   const static time_point ct{ microseconds{ static_cast<int64_t>( current_time() ) } };
   return ct;
}
