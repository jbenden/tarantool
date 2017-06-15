env = require('test_run')
test_run = env.new()

fiber = require'fiber'

-- simple test for parallel ddl execution
_ = box.schema.space.create('test'):create_index('pk')

ch = fiber.channel(2)

test_run:cmd("setopt delimiter ';'")

function f1()
    box.space.test:create_index('sec', {parts = {2, 'num'}})
    ch:put(true)
end;

function f2()
    box.space.test:create_index('third', {parts = {3, 'string'}})
    ch:put(true)
end;

test_run:cmd("setopt delimiter ''");

_ = {fiber.create(f1), fiber.create(f2)}

ch:get()
ch:get()

_ = box.space.test:drop()

test_run:cmd('restart server default')

env = require('test_run')
test_run = env.new()
fiber = require'fiber'

ch = fiber.channel(3)

_ = box.schema.space.create('test'):create_index('pk')

test_run:cmd("setopt delimiter ';'")
function add_index()
    box.space.test:create_index('sec', {parts = {2, 'num'}})
    ch:put(true)
end;

function insert_tuple(tuple)
    ch:put({pcall(box.space.test.replace, box.space.test, tuple)})
end;
test_run:cmd("setopt delimiter ''");

_ = {fiber.create(insert_tuple, {1, 2, 'a'}), fiber.create(add_index), fiber.create(insert_tuple, {2, '3', 'b'})}
{ch:get(), ch:get(), ch:get()}

box.space.test:select()
box.space.test:drop()

test_run:cmd('restart server default')

env = require('test_run')
test_run = env.new()
fiber = require'fiber'
errinj = box.error.injection

_ = box.schema.space.create('test'):create_index('pk')
box.space.test:replace({1, 2, 3, 4})

errinj.set("ERRINJ_WAL_WRITE", true)
fiber.sleep(0.01)
box.space.test.index.pk:rename('new_pk')
errinj.set("ERRINJ_WAL_WRITE", false)
box.space.test:select()

sec = box.space.test:create_index('sec', {parts = {2, 'num'}})

ch = fiber.channel()

test_run:cmd("setopt delimiter ';'")
function drop_index()
    ch:put({pcall(sec.drop, sec)})
end;

function insert_tuple(tuple)
    ch:put({pcall(box.space.test.replace, box.space.test, tuple)})
end;
test_run:cmd("setopt delimiter ''");

errinj.set("ERRINJ_WAL_WRITE", true)
fiber.sleep(0.01)
_ = {fiber.create(insert_tuple, {1, '2', 'a'}), fiber.create(drop_index), fiber.create(insert_tuple, {2, '3', 'b'})}
{ch:get(), ch:get(), ch:get()}
errinj.set("ERRINJ_WAL_WRITE", false)
box.space.test:select()
box.space.test:replace({3, 'a', 'b'})
box.space.test:drop()
