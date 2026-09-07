-- 东软电动汽车充电桩应用管理平台 —— 数据库结构参考文件
-- 对应《概要设计说明书》第3.3节
-- 可以直接用 sqlite3 charging.db < schema.sql 建库，
-- 也可以只当作参考，实际建表由 database.cpp 里的代码在程序启动时自动完成

create table if not exists users(
    id integer primary key autoincrement,
    phone text unique not null,
    nickname text,
    avatar_path text,
    balance real default 0,
    status text default '正常',
    created_at datetime default current_timestamp
);

create table if not exists stations(
    id integer primary key autoincrement,
    name text not null,
    address text,
    longitude real,
    latitude real,
    price real,
    pile_count integer default 0
);

create table if not exists piles(
    id integer primary key autoincrement,
    station_id integer,
    code text,
    type text,
    power real,
    status text default '闲置',
    total_sessions integer default 0,
    total_duration integer default 0,
    foreign key(station_id) references stations(id)
);

create table if not exists orders(
    id integer primary key autoincrement,
    user_id integer,
    pile_id integer,
    start_time datetime,
    end_time datetime,
    amount real,
    fee real,
    status text default '充电中',
    foreign key(user_id) references users(id),
    foreign key(pile_id) references piles(id)
);

create table if not exists admins(
    id integer primary key autoincrement,
    username text unique not null,
    password text not null
);

create table if not exists login_logs(
    id integer primary key autoincrement,
    phone text not null,
    login_time datetime default current_timestamp,
    ip_address text
);

create table if not exists operation_logs(
    id integer primary key autoincrement,
    operator_id integer,
    operator_type text,
    operation_type text,
    target_table text,
    target_id integer,
    content text,
    operation_time datetime default current_timestamp
);
