#!/usr/bin/env tarantool
test = require("sqltester")
test:plan(17)

--!./tcltestrunner.lua
-- 2009 December 29
--
-- The author disclaims copyright to this source code.  In place of
-- a legal notice', here is a blessing:
--
--    May you do good and not evil.
--    May you find forgiveness for yourself and forgive others.
--    May you share freely, never taking more than you give.
--
-------------------------------------------------------------------------
--
-- This file tests the effects of SQL variable references embedded in
-- triggers. If the user attempts to create such a trigger, it is an
-- error. However, if an existing trigger definition is read from
-- the sqlite_master table, the variable reference always evaluates
-- to NULL.
--
-- ["set","testdir",[["file","dirname",["argv0"]]]]
-- ["source",[["testdir"],"\/tester.tcl"]]


testprefix = "triggerE"
test:do_execsql_test(
    1.0,
    [[
        CREATE TABLE t1(a PRIMARY KEY, b);
        CREATE TABLE t2(c  PRIMARY KEY, d);
        CREATE TABLE t3(e  PRIMARY KEY, f);
    ]])

-- forcedelete test.db2
-- do_execsql_test 1.1 {
--   ATTACH 'test.db2' AS aux;
--   CREATE TABLE aux.t4(x);
--   INSERT INTO aux.t4 VALUES(5);
--
--   CREATE TRIGGER tr1 AFTER INSERT ON t1 WHEN new.a IN (SELECT x FROM aux.t4)
--   BEGIN
--     SELECT 1;
--   END;
-- }
-- do_execsql_test 1.2 { INSERT INTO t1 VALUES(1,1); }
-- do_execsql_test 1.3 { INSERT INTO t1 VALUES(5,5); }
---------------------------------------------------------------------------
-- Attempt to create various triggers that use bound variables.
--
local errmsg = "trigger cannot use variables"
for tn, defn in ipairs({
        "AFTER INSERT ON t1 WHEN new.a = ? BEGIN SELECT 1; END; ",
        "BEFORE DELETE ON t1 BEGIN SELECT ?; END; ",
        "BEFORE DELETE ON t1 BEGIN SELECT * FROM (SELECT * FROM (SELECT ?)); END; ",
        "BEFORE DELETE ON t1 BEGIN SELECT * FROM t2 GROUP BY ?; END; ",
        "BEFORE DELETE ON t1 BEGIN SELECT * FROM t2 LIMIT ?; END; ",
        "BEFORE DELETE ON t1 BEGIN SELECT * FROM t2 ORDER BY ?; END; ",
        "BEFORE UPDATE ON t1 BEGIN UPDATE t2 SET c = ?; END; ",
        "BEFORE UPDATE ON t1 BEGIN UPDATE t2 SET c = 1 WHERE d = ?; END; "}) do

-- for _ in X(0, "X!foreach", [=[["tn defn","\n  1 { AFTER INSERT ON t1 WHEN new.a = ? BEGIN SELECT 1; END; }\n  2 { BEFORE DELETE ON t1 BEGIN SELECT ?; END; }\n  3 { BEFORE DELETE ON t1 BEGIN SELECT * FROM (SELECT * FROM (SELECT ?)); END; }\n  5 { BEFORE DELETE ON t1 BEGIN SELECT * FROM t2 GROUP BY ?; END; }\n  6 { BEFORE DELETE ON t1 BEGIN SELECT * FROM t2 LIMIT ?; END; }\n  7 { BEFORE DELETE ON t1 BEGIN SELECT * FROM t2 ORDER BY ?; END; }\n  8 { BEFORE UPDATE ON t1 BEGIN UPDATE t2 SET c = ?; END; }\n  9 { BEFORE UPDATE ON t1 BEGIN UPDATE t2 SET c = 1 WHERE d = ?; END; }\n"]]=]) do
    test:catchsql "drop trigger tr1"
    test:do_catchsql_test(
        "1.1."..tn,
        "CREATE TRIGGER tr1 "..defn.."", {
            1, errmsg
        })

    test:do_catchsql_test(
        "1.2."..tn,
        "CREATE TEMP TRIGGER tr1 "..defn.."", {
            1, errmsg
        })
end
---------------------------------------------------------------------------
-- Test that variable references within trigger definitions loaded from
-- the sqlite_master table are automatically converted to NULL.
--
-- do_execsql_test 2.1 {
--   PRAGMA writable_schema = 1;
--   INSERT INTO sqlite_master VALUES('trigger', 'tr1', 't1', 0,
--     'CREATE TRIGGER tr1 AFTER INSERT ON t1 BEGIN
--         INSERT INTO t2 VALUES(?1, ?2);
--      END'
--   );
--   INSERT INTO sqlite_master VALUES('trigger', 'tr2', 't3', 0,
--     'CREATE TRIGGER tr2 AFTER INSERT ON t3 WHEN ?1 IS NULL BEGIN
--         UPDATE t2 SET c=d WHERE c IS ?2;
--      END'
--   );
-- }
-- db close
-- sqlite3 db test.db
-- do_execsql_test 2.2.1 {
--   INSERT INTO t1 VALUES(1, 2);
--   SELECT * FROM t2;
-- } {{} {}}
-- do_test 2.2.2 {
--   set one 3
--   execsql {
--     DELETE FROM t2;
--     INSERT INTO t1 VALUES($one, ?1);
--     SELECT * FROM t2;
--   }
-- } {{} {}}
-- do_execsql_test 2.2.3 { SELECT * FROM t1 } {1 2 3 3}
-- do_execsql_test 2.3 {
--   DELETE FROM t2;
--   INSERT INTO t2 VALUES('x', 'y');
--   INSERT INTO t2 VALUES(NULL, 'z');
--   INSERT INTO t3 VALUES(1, 2);
--   SELECT * FROM t3;
--   SELECT * FROM t2;
-- } {1 2 x y z z}
test:finish_test()

