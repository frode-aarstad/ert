import json
import logging
import os
import stat
from pathlib import Path

import pytest

from ert.scheduler import LsfDriver
from tests.utils import poll

from .conftest import mock_bin


@pytest.fixture(autouse=True)
def mock_lsf(pytestconfig, monkeypatch, tmp_path):
    if pytestconfig.getoption("lsf"):
        # User provided --lsf, which means we should use the actual LSF
        # cluster without mocking anything.""
        return
    mock_bin(monkeypatch, tmp_path)


@pytest.fixture
def not_found_bjobs(monkeypatch, tmp_path):
    """This creates a bjobs command that will always claim a job
    does not exist, mimicking a job that has 'fallen out of the bjobs cache'."""
    os.chdir(tmp_path)
    bin_path = tmp_path / "bin"
    bin_path.mkdir()
    monkeypatch.setenv("PATH", f"{bin_path}:{os.environ['PATH']}")
    bjobs_path = bin_path / "bjobs"
    bjobs_path.write_text(
        "#!/bin/sh\n" 'echo "Job <$1> is not found"',
        encoding="utf-8",
    )
    bjobs_path.chmod(bjobs_path.stat().st_mode | stat.S_IEXEC)


async def test_lsf_stdout_file(tmp_path, job_name):
    os.chdir(tmp_path)
    driver = LsfDriver()
    await driver.submit(0, "sh", "-c", "echo yay", name=job_name)
    await poll(driver, {0})
    lsf_stdout = Path(f"{job_name}.LSF-stdout").read_text(encoding="utf-8")
    assert Path(
        f"{job_name}.LSF-stdout"
    ).exists(), "LSF system did not write output file"

    assert "Sender: " in lsf_stdout, "LSF stdout should always start with 'Sender:'"
    assert "The output (if any) follows:" in lsf_stdout
    assert "yay" in lsf_stdout


async def test_lsf_dumps_stderr_to_file(tmp_path, job_name):
    os.chdir(tmp_path)
    driver = LsfDriver()
    failure_message = "failURE"
    await driver.submit(0, "sh", "-c", f"echo {failure_message} >&2", name=job_name)
    await poll(driver, {0})
    assert Path(
        f"{job_name}.LSF-stderr"
    ).exists(), "LSF system did not write stderr file"

    assert (
        Path(f"{job_name}.LSF-stderr").read_text(encoding="utf-8").strip()
        == failure_message
    )


@pytest.mark.parametrize("explicit_runpath", [(True), (False)])
async def test_lsf_info_file_in_runpath(explicit_runpath, tmp_path, job_name):
    os.chdir(tmp_path)
    driver = LsfDriver()
    (tmp_path / "some_runpath").mkdir()
    os.chdir(tmp_path)
    effective_runpath = tmp_path / "some_runpath" if explicit_runpath else tmp_path
    await driver.submit(
        0,
        "sh",
        "-c",
        "exit 0",
        runpath=tmp_path / "some_runpath" if explicit_runpath else None,
        name=job_name,
    )

    await poll(driver, {0})

    effective_runpath = tmp_path / "some_runpath" if explicit_runpath else tmp_path
    assert json.loads(
        (effective_runpath / "lsf_info.json").read_text(encoding="utf-8")
    ).keys() == {"job_id"}


@pytest.mark.integration_test
async def test_submit_to_named_queue(tmp_path, caplog, job_name):
    """If the environment variable _ERT_TEST_ALTERNATIVE_QUEUE is defined
    a job will be attempted submitted to that queue.

    As Ert does not keep track of which queue a job is executed in, we can only
    test for success for the job."""
    os.chdir(tmp_path)
    driver = LsfDriver(queue_name=os.getenv("_ERT_TESTS_ALTERNATIVE_QUEUE"))
    await driver.submit(0, "sh", "-c", f"echo test > {tmp_path}/test", name=job_name)
    await poll(driver, {0})

    assert (tmp_path / "test").read_text(encoding="utf-8") == "test\n"


@pytest.mark.usefixtures("use_tmpdir")
async def test_submit_with_resource_requirement(job_name):
    resource_requirement = "select[cs && x86_64Linux]"
    driver = LsfDriver(resource_requirement=resource_requirement)
    await driver.submit(0, "sh", "-c", "echo test>test", name=job_name)
    await poll(driver, {0})

    assert Path("test").read_text(encoding="utf-8") == "test\n"


async def test_polling_bhist_fallback(not_found_bjobs, caplog, job_name):
    caplog.set_level(logging.DEBUG)
    driver = LsfDriver()
    Path("mock_jobs").mkdir()
    Path("mock_jobs/pendingtimemillis").write_text("100")
    driver._poll_period = 0.01

    bhist_called = False
    original_bhist_method = driver._poll_once_by_bhist

    def mock_poll_once_by_bhist(*args, **kwargs):
        nonlocal bhist_called
        bhist_called = True
        return original_bhist_method(*args, **kwargs)

    driver._poll_once_by_bhist = mock_poll_once_by_bhist

    await driver.submit(0, "sh", "-c", "sleep 1", name=job_name)
    job_id = list(driver._iens2jobid.values())[0]
    await poll(driver, {0})
    assert "bhist is used" in caplog.text
    assert bhist_called
    assert job_id in driver._bhist_cache
