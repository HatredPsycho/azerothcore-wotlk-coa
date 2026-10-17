# ReagentBankUI

The required interface for [mod-reagent-bank-account](../../modules/mod-reagent-bank-account/README.md): browse,
deposit and withdraw the server-side reagent bank, withdraw a recipe's missing reagents from a profession window and
keep an Auction House shopping list. A launcher button sits below the character frame; no banker NPC is needed.

## Installation

Copy this entire `ReagentBankUI` directory into the client's `Interface/AddOns` directory. The server side is
switched by `ReagentBankAccount.Enable` in `mod_reagent_bank_account.conf`; storage is account-wide by default
(`ReagentBankAccount.AccountWide`).

The addon talks to the server through the `.rbank` command and reads the hidden `RBANK:*` system lines it returns.
Without the addon those lines would show in chat.
