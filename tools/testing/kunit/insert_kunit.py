#! /usr/bin/env python3
'''
Insert a default KUnit unit test templete in the directory
The result will be:
mydir$ tree
.
├── my_driver.c
├── Kconfig
├── Makefile
└── kunit_test
    ├── Makefile
    └── my_driver_test.c
'''

__author__ = 'Ji-Hun Kim'
__version__ = '0.1.0'
__maintainer__ = 'Ji-Hun Kim'
__email__ = 'ji_hun.kim@samsung.com'

import argparse
import sys
import os
import re
from kunit_new_template import create_skeletons, Skeletons

TEST_PATH='kunit_test'

class ConfigNameExtractor():
    def __init__(self, path):
        dir_name, file_name = os.path.split(path)
        mkfile = os.path.join(dir_name, 'Makefile')
        file_prefix, _ = os.path.splitext(file_name)
        obj_name = file_prefix + '.o'
        self.result = None
        self.__search_file(obj_name, mkfile)

    def __search_file(self, string, target_file):
        if not os.path.exists(target_file):
            print('The file is not exist: %s' %target_file)
            sys.exit(1)
        with open(target_file, 'rt') as fp:
            for line in fp:
                ret = self.__search_line(string, line)

    def __search_line(self, string, line):
        if string in line:
            self.__extract_config(line)

    def __extract_config(self, line):
        # FIXME: is there another cases?
        search = 'obj-\$\((.*?)\)' # get inside of obj-$()
        rgx = re.compile(search)
        srch = rgx.search(line)
        if srch:
            self.result = srch.group(1)

class TemplateGenerator():
    def __init__(self, path, postfix='-test'):
        self.__create_path(path, postfix)

    def __create_path(self, path, postfix):
        self.dir_name, self.file_name = os.path.split(path)
        self.file_prefix, _ = os.path.splitext(self.file_name)
        self.testfile_name = os.path.join(self.dir_name, self.file_prefix + postfix + '.c')
        self.test_object_file = self.file_prefix + postfix + '.o'
        # FIXME: change to CONFIG_KUNIT_<file name upper case>_TEST
        # from CONFIG_<file name upper case>_TEST
        # it is generated by class create_skeletons
        namespace_prefix = self.file_prefix.replace('-', '_')
        self.kconfig_name = 'CONFIG_' + namespace_prefix.upper() + '_TEST'
        self.skeletons = create_skeletons(namespace_prefix, self.test_object_file)

    def get_kconfig(self):
        return self.skeletons.kconfig_skeleton

    def get_makefile(self):
        return self.skeletons.makefile_skeleton

    def get_testfile(self):
        return self.skeletons.test_skeleton

class TestConstructor():
    '''
    modify path/Makefile
    modify path/Kconfig
    mkdir path/kunit_test
    gen path/kunit_test/Makefile
    gen path/kunit_test/testfile.c
    '''
    def __init__(self, path):
        self.tmplts = TemplateGenerator(path)
        self.dir_name = self.tmplts.dir_name
        self.file_prefix = self.tmplts.file_prefix
        self.test_path = os.path.join(self.dir_name, TEST_PATH)
        self.test_driver_name = self.file_prefix + '_test'
        self.tgt_config_name = ConfigNameExtractor(path).result
        if os.path.exists(self.test_path):
            self.additional = True
        else:
            os.mkdir(self.test_path)
            self.additional = False

    def __print_result(self):
        pass

    def __search_str(self, content, target):
        with open(target, 'r') as fp:
            ret = False
            if content in fp.read():
                ret = True
        return ret

    def __is_contain(self, string, fpath):
        def strip(orig):
            return ' '.join(orig.split())
        with open(fpath, 'r') as fp:
            if strip(string) in strip(fp.read()):
                return True
            else:
                return False

    def __cat_file(self, f):
        with open(f, 'r') as fp:
            print(fp.read())

    def __append_content(self, content, target, debug=False):
        if not os.path.exists(target):
            print('not exist: %s' %target)
            sys.exit(1)
        if self.__is_contain(content, target):
            return

        with open(target, 'a') as fp:
            #if not self.__search_str(content, target):
            fp.write(content)
        if debug:
            print('appended: %s' %target)
            self.__cat_file(target)

    def __write_content(self, content, target, debug=False):
        if os.path.exists(target):
            if self.__is_contain(content, target):
                return
            with open(target, 'a') as fp:
                fp.write(content)
        else:
            with open(target, 'w') as fp:
                fp.write(content)

        if debug:
            print('generated: %s' %target)
            self.__cat_file(target)

    def append_makefile(self):
        mk_path = os.path.join(self.dir_name, 'Makefile')
        mk_content = '\nobj-$(CONFIG_KUNIT)\t\t\t+= %s/\n' %TEST_PATH
        gcov_content = 'GCOV_PROFILE_' + self.file_prefix + '.o\t\t:=y\n'

        self.__append_content(mk_content, mk_path)
        self.__append_content(gcov_content, mk_path)

    def append_kconfig(self):
        mk_path = os.path.join(self.dir_name, 'Kconfig')
        test_kconfig = self.tmplts.kconfig_name.replace('CONFIG_', '')
        # FIXME: the config name should be a real config + _TEST ?
        if self.__is_contain('config %s' %test_kconfig, mk_path):
            return
        self.__append_content('\n' + self.tmplts.get_kconfig() + '\n', mk_path)

    def write_test_makefile(self):
        mk_path = os.path.join(self.test_path, 'Makefile')
        obj_name = self.test_driver_name + '.o'
        test_config = self.tmplts.kconfig_name
        if self.tgt_config_name:
            tgt_config = '$(%s)' %(self.tgt_config_name)
        else:
            tgt_config = 'y'
        mk_contents = [
                'obj-%s\t\t+= %s\n' %(tgt_config, obj_name),
                'obj-$(%s)\t\t+= %s\n' %(test_config, obj_name)
                ]
        for line in mk_contents:
            self.__write_content(line, mk_path)

    def write_test_driver(self):
        drv_path = os.path.join(self.test_path, self.test_driver_name + '.c')
        drv_cont = self.tmplts.get_testfile()
        self.__write_content(drv_cont, drv_path)

def parse_args():
    parser = argparse.ArgumentParser(description='Generate a KUnit test template')
    parser.add_argument('-f', '--filepath', dest='file_path', help='unit testing target file path')
    args = parser.parse_args()
    return args

def main():
    arg = parse_args()
    a = TestConstructor(os.path.abspath(arg.file_path))
    a.append_makefile()
    a.append_kconfig()
    a.write_test_makefile()
    a.write_test_driver()
    result = os.path.join(os.path.split(arg.file_path)[0], TEST_PATH, a.test_driver_name + '.c')
    print('Done! You can write test cases on the %s' %result)

if __name__ == '__main__':
    main()
