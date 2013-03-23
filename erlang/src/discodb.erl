-module(discodb).

%% DiscoDBCons

-export([cons/0,
         add/2,
         add/3,
         finalize/1,
         finalize/2]).

%% DiscoDB

-export([new/1,
         new/2,
         load/1,
         loads/1,
         dump/2,
         dumps/1,
         query/2,
         get/2,
         keys/1,
         values/1,
         unique_values/1]).

%% DiscoDBIter

-export([fold/3,
         next/1,
         size/1,
         to_list/1]).

str(Bin) when is_binary(Bin) ->
    binary_to_list(Bin);
str(Str) when is_list(Str) ->
    Str.

init(Type, Func, Args) ->
    case discodb_nif:init(Type, Func, Args) of
        DDB when is_binary(DDB) ->
            receive
                ok ->
                    DDB;
                Reply ->
                    Reply
            end;
        Error ->
            Error
    end.

call(DDB, Method, Args) ->
    case discodb_nif:call(DDB, Method, Args) of
        DDB ->
            receive
                ok ->
                    DDB;
                Reply ->
                    Reply
            end;
        Error ->
            Error
    end.

%% DiscoDBCons

cons() ->
    init(discodb_cons, new, []).

add(Cons, Item) ->
    call(Cons, add, Item).

add(Cons, Key, Val) ->
    add(Cons, {Key, Val}).

finalize(Cons) ->
    finalize(Cons, []).

finalize(Cons, Flags) ->
    call(Cons, finalize, Flags).

%% DiscoDB

new(Items) ->
    new(Items, []).

new(Items, Flags) ->
    finalize(lists:foldl(fun ({Key, Val}, Cons) ->
                                 add(Cons, {Key, Val})
                         end, cons(), Items), Flags).

load(Filename) ->
    init(discodb, load, str(Filename)).

loads(Data) ->
    init(discodb, loads, str(Data)).

dump(DB, Filename) ->
    call(DB, dump, str(Filename)).

dumps(DB) ->
    call(DB, dumps, []).

get(DB, Key) ->
    call(DB, get, Key).

keys(DB) ->
    call(DB, iter, keys).

values(DB) ->
    call(DB, iter, values).

unique_values(DB) ->
    call(DB, iter, unique_values).

query(DB, Q) ->
    call(DB, query, Q). %% XXX

%% DiscoDBIter

fold(Iter, Fun, Acc) ->
    case discodb:next(Iter) of
        null ->
            Acc;
        <<Entry/binary>> ->
            fold(Iter, Fun, Fun(Entry, Acc))
    end.

next(Iter) ->
    discodb_nif:next(Iter).

size(Iter) ->
    discodb_nif:size(Iter).

to_list(Iter) ->
    lists:reverse(fold(Iter, fun (E, Acc) -> [E|Acc] end, [])).
