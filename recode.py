#!/usr/bin/python
# -*- coding: utf-8 -*-
import io, os, re, shutil, sys
import chardet
import codecs
import datetime

#注意需要安装chardet模块

today = datetime.datetime.now().strftime("%Y-%m-%d");

#检测文件编码,如果不是的话,统一改为utf-8
#将table转换为space
def recode(path):
    raw = open(path, 'rb').read();
    if raw.startswith(codecs.BOM_UTF8):
        encoding = 'utf-8-sig'
    else:
        result = chardet.detect(raw)
        encoding = result['encoding']

    lines = io.open(path, "r", encoding=encoding).readlines();
    for i in range(0, len(lines)):
        lines[i] = lines[i].rstrip().expandtabs(4) + "\n";
    io.open(path, "w", encoding="utf-8-sig").writelines(lines);

def load_config():
	lines = open(".git/config").readlines();
	find_section = False;
	for line in lines:
		line = line.strip(" \t\r\n");
		if line == "[remote \"origin\"]":
			find_section = True;
		elif find_section:
			tokens = line.split("=");
			return tokens[1].strip();
	return None;

rep_name = load_config();
if rep_name == None:
	sys.exit("没找到.git配置,必须在git仓库根目录运行!");

sign = list();
sign.append(u"/*");
sign.append(u"** repository: %s" % rep_name);
sign.append(u"** trumanzhao, %s, trumanzhao@foxmail.com" % today);
sign.append(u"*/");
sign.append(u"");

def sign_file(path):
    recode(path);
    lines = io.open(path, "r", encoding="utf-8-sig").readlines();
    if  len(lines) > 2 and re.match(".+repository.+github.+", lines[1]):
        print("%s 已签名!" % path);
        return;

    for i in range(0, len(sign)):
        lines.insert(i, sign[i] + u"\n");

    print("加签名: %s" % path);
    io.open(path, "w", encoding="utf-8").writelines(lines);

root = ".";
items = os.listdir(root);
for item in items:
    path = os.path.join(root, item);
    ext = os.path.splitext(path)[1].lower();
    if ext == ".cpp" or ext == ".h":
        sign_file(path);

