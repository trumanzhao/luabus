#!/usr/bin/python
#coding: utf-8

import os, sys, getopt, commands, re;
import service;

def get_target_apps():
    argc = len(sys.argv);
    if argc >= 2:
        return sys.argv[1:];
    return service.apps;


def run_app_one(app):
    cmd = "./hive";
    cmd += " %s.lua" % app;
    cmd += " --index=1";
    cmd += " --daemon";

    if app == "router":
        cmd += " --listen=127.0.0.1:9000";
    else
        cmd += " --routers=127.0.0.1:9000";

    if app == "gateway":
        cmd += " --listen=127.0.0.1:9001";
    if app == "gamesvr":
        cmd += " --listen=127.0.0.1:9002";
    print(cmd);

    os.system(cmd);

apps = get_target_apps();
for app in apps:
    pids = mgr.find_pid_list(app);
    for pid in pids:
        cmd = "kill -9 %d" % pid;
        print(cmd);
        os.system(cmd);

# 如果没有任何进程存在，则先删除log目录
pids = service.find_all_app();
if len(pids) == 0:
    os.system("rm -rf log");
    os.system("rm -f *.err");
    os.system("rm -f *.log");

for app in apps:
    run_app_one(app);




