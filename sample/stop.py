#!/usr/bin/python
#coding: utf-8
import sys, os, getopt, commands, service;

#根据参数判断需要kill的app,返回名称列表
def get_target_apps():
    argc = len(sys.argv);
    if argc >= 2:
        return sys.argv[1:];
    return service.apps;

apps = get_target_apps();
for app in apps:
    pids = service.find_pid_list(app);
    for pid in pids:
        print("kill %d" % pid);
        os.system("kill %d" % pid);

