from platformio.test.result import TestCase, TestStatus
from platformio.test.runners.base import TestRunnerBase


class CustomTestRunner(TestRunnerBase):
    def on_testing_line_output(self, line):
        if line.startswith("PASS: "):
            self.test_suite.add_case(
                TestCase(name=line[6:].strip(), status=TestStatus.PASSED)
            )
        super().on_testing_line_output(line)
