-module(discodb_nif).

-export([init/3, call/3]).
-export([next/1, size/1]).

-define(nif_not_loaded, erlang:nif_error({nif_not_loaded, module, ?MODULE, line, ?LINE})).

-on_load(load_nif/0).
load_nif() ->
    PrivDir = case code:priv_dir(?MODULE) of
                  {error, bad_name} ->
                      EbinDir = filename:dirname(code:which(?MODULE)),
                      AppPath = filename:dirname(EbinDir),
                      filename:join(AppPath, "priv");
                  Path ->
                      Path
              end,
    erlang:load_nif(filename:join(PrivDir, ?MODULE), 0).

init(_Type, _Fun, _Args) ->
    ?nif_not_loaded.

call(_Obj, _Method, _Args) ->
    ?nif_not_loaded.

next(_Iter) ->
    ?nif_not_loaded.

size(_Iter) ->
    ?nif_not_loaded.
