import unittest

from import_app2repo import run_command_remote
from import_app2repo import push_files_remote

class MyTestCase(unittest.TestCase):
    def test_something(self):
        with open("/tmp/1.txt","a+") as f:
            f.write("")
        with open("/tmp/2","a+") as f2:
            f2.write("")
        run_command_remote(["ls", "-aslh"])
        run_command_remote(["mkdir", "-p", "/tmp/x2/"])
        push_files_remote(["/tmp/1.txt", "/tmp/2"], "/tmp/x2/")
        run_command_remote(["ls " " -alh", "/tmp/x2/"])
        self.assertEqual(True, True)  # add assertion here

if __name__ == '__main__':
    unittest.main()
