#!/usr/bin/env python3

import argparse
import atexit
import datetime
import errno
import os
import psutil
import signal
import subprocess
import sys


test_matrix = [
    {
        "model": "iris",
        "test_filter": "[multicopter]",
        "timeout_min": 10,
    },
    {
        "model": "standard_vtol",
        "test_filter": "[vtol]",
        "timeout_min": 10,
    }
]


class Runner:
    def __init__(self, log_dir):
        self.cmd = ""
        self.cwd = None
        self.args = []
        self.env = {}
        self.log_prefix = ""
        self.log_dir = log_dir

    def start(self, config):
        if self.log_dir:
            f = open(self.log_dir + os.path.sep +
                     "log-{}-{}-{}-{}.txt".format(
                         self.log_prefix,
                         config['model'],
                         config['test_filter'],
                         datetime.datetime.now().strftime("%Y-%m-%dT%H-%M-%SZ")
                        ), 'w')
        else:
            f = sys.stdout

        print("Running: {}".format(" ".join([self.cmd] + self.args)))

        self.process = subprocess.Popen(
            [self.cmd] + self.args,
            cwd=self.cwd,
            env=self.env,
            stdout=f, stderr=f
        )

        atexit.register(self.stop)

    def wait(self, timeout_min):
        try:
            return self.process.wait(timeout=timeout_min*60)
        except subprocess.TimeoutExpired:
            print("Timeout of {} min{} reached, stopping...".
                  format(timeout_min, "s" if timeout_min > 1 else ""))
            self.stop()
            print("stopped.")
            return errno.ETIMEDOUT

    def stop(self):
        atexit.unregister(self.stop)

        returncode = self.process.poll()
        if returncode is not None:
            return returncode

        print("Sending SIGINT to {}".format(self.process.pid))
        self.process.send_signal(signal.SIGINT)
        try:
            return self.process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            pass

        print("Sending SIGTERM to {}".format(self.process.pid))
        self.process.terminate()

        try:
            return self.process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            pass

        print("Sending SIGKILL to {}".format(self.process.pid))
        self.process.kill()
        return self.process.returncode


class Px4Runner(Runner):
    def __init__(self, workspace_dir, log_dir, speed_factor):
        super().__init__(log_dir)
        self.cmd = workspace_dir + "/build/px4_sitl_default/bin/px4"
        self.cwd = workspace_dir + "/build/px4_sitl_default/tmp/rootfs"
        self.args = [
                workspace_dir + "/ROMFS/px4fmu_common",
                "-s",
                "etc/init.d-posix/rcS",
                "-t",
                workspace_dir + "/test_data",
                "-d"
            ]
        self.env = {"PATH": os.environ['PATH'],
                    "PX4_SIM_MODEL": "iris",
                    "PX4_SIM_SPEED_FACTOR": speed_factor}
        self.log_prefix = "px4"


class GazeboRunner(Runner):
    def __init__(self, workspace_dir, log_dir, speed_factor):
        super().__init__(log_dir)
        self.env = {"PATH": os.environ['PATH'],
                    "HOME": os.environ['HOME'],
                    "GAZEBO_PLUGIN_PATH":
                    workspace_dir + "/build/px4_sitl_default/build_gazebo",
                    "GAZEBO_MODEL_PATH":
                    workspace_dir + "/Tools/sitl_gazebo/models",
                    "PX4_SIM_SPEED_FACTOR": speed_factor}
        self.cmd = "gzserver"
        self.args = ["--verbose",
                     workspace_dir + "/Tools/sitl_gazebo/worlds/iris.world"]
        self.log_prefix = "gazebo"


class TestRunner(Runner):
    def __init__(self, workspace_dir, log_dir, config, test):
        super().__init__(log_dir)
        self.env = {"PATH": os.environ['PATH']}
        self.cmd = workspace_dir + \
            "/build/px4_sitl_default/mavsdk_tests"
        self.args = [test]
        self.log_prefix = "test_runner"


def determine_tests(workspace_dir, filter):
    cmd = workspace_dir + "/build/px4_sitl_default/mavsdk_tests"
    args = ["--list-test-names-only", filter]
    p = subprocess.Popen(
        [cmd] + args,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    tests = str(p.stdout.read().decode("utf-8")).strip().split('\n')
    return tests


def is_running(process_name):
    for proc in psutil.process_iter(attrs=['name']):
        if proc.info['name'] == process_name:
            True
    return False


def is_everything_ready():
    result = True
    if is_running('px4'):
        print("px4 process already running\n"
              "run `killall px4` and try again")
        result = False
    if is_running('gzserver'):
        print("gzserver process already running\n"
              "run `killall gzserver` and try again")
        result = False
    if not os.path.isfile('build/px4_sitl_default/bin/px4'):
        print("PX4 SITL is not built\n"
              "run `PX4_MAVSDK_TESTING=y DONT_RUN=1 "
              "make px4_sitl gazebo mavsdk_tests`")
    if not os.path.isfile('build/px4_sitl_default/mavsdk_tests'):
        print("Test runner is not built\n"
              "run `PX4_MAVSDK_TESTING=y DONT_RUN=1 "
              "make px4_sitl gazebo mavsdk_tests`")
        result = False
    return result


def main():

    parser = argparse.ArgumentParser()
    parser.add_argument("--log-dir",
                        help="Directory for log files, stdout if not provided")
    parser.add_argument("--speed-factor", default=1,
                        help="How fast to run the simulation")
    args = parser.parse_args()

    if not is_everything_ready():
        return

    overall_success = True

    for group in test_matrix:
        print("Running test group for '{}' with filter '{}'"
              .format(group['model'], group['test_filter']))

        tests = determine_tests(os.getcwd(), group['test_filter'])

        for test in tests:

            print("Running test '{}'".format(test))

            px4_runner = Px4Runner(
                os.getcwd(), args.log_dir, args.speed_factor)
            px4_runner.start(group)

            gazebo_runner = GazeboRunner(
                os.getcwd(), args.log_dir, args.speed_factor)
            gazebo_runner.start(group)

            test_runner = TestRunner(os.getcwd(), args.log_dir, group, test)
            test_runner.start(group)

            returncode = test_runner.wait(group['timeout_min'])
            was_success = (returncode == 0)
            print("Test '{}': {}".
                  format(test, "Success" if was_success else "Fail"))
            if not was_success:
                overall_success = False

            returncode = gazebo_runner.stop()
            print("Gazebo exited with {}".format(returncode))

            px4_runner.stop()
            print("PX4 exited with {}".format(returncode))

    print("Overall result: {}".
          format("SUCCESS" if overall_success else "FAIL"))
    return 0 if overall_success else 1


if __name__ == '__main__':
    main()
