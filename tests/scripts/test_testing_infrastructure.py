import attr
import pytest
from afni_test_utils import tools
from pathlib import Path
import shutil
from datalad import api as datalad
import os
import asyncio


async def make_pretend_repo(dirname):
    os.chdir(dirname)
    datalad.create(str(dirname), force=True)
    rev_log = datalad.save("add data", str(dirname))
    (dirname / "useless.txt").write_text("who me.")
    datalad.save("add superfluous change", str(dirname))
    (dirname / "useless.txt").unlink()
    datalad.save("make things better", str(dirname))


def get_outdir(dirname):
    tname = "test_function_name"
    return Path(dirname) / "toolname" / tname


def create_data_dir(dirname):
    tooldir = dirname / "toolname"
    tooldir.mkdir()
    testdir = tooldir / "test_function_name"
    testdir.mkdir()
    logdir = testdir / "captured_output"
    logdir.mkdir()
    outdir = get_outdir(dirname)
    test_comparison_dir = outdir

    sample_text = """
        The following lines should be part of a comparison for text files but not logs:
        AFNI version=
        Clock time now
        elapsed time
        auto-generated by
        CPU time =

        Some more text in the file...
        An absolute path:
        /path/afni_code/tests/output_of_tests/output_date/module/testname/outputfile.txt

        An absolute path to test data:
        /path/afni_code/tests/afni_ci_test_data/mini_data/some_nii.gz
        """

    cmd_info = {
        "host": "hostname",
        "user": "username",
        "cmd": "a command",
        "workdir": tooldir.parent.parent,
    }

    out_dict = {
        "module_outdir": outdir.parent,
        "sampdir": tools.convert_to_sample_dir_path(dirname),
        "logdir": outdir / "captured_output",
        "comparison_dir": test_comparison_dir,
        "base_comparison_dir": mock_data_orig,
        "base_outdir": dirname,
        "tests_data_dir": dirname.parent.parent / "test_data_dir",
        "outdir": outdir,
        "test_name": outdir.name,
    }
    cmd_info.update(out_dict)

    # Write some logs and a text file
    stdout = logdir / (testdir.name + "_stdout.log")
    stdout.write_text(sample_text)
    Path(str(stdout).replace("stdout", "stderr")).write_text(sample_text)
    tools.write_command_info(Path(str(stdout).replace("stdout", "cmd")), cmd_info)
    (testdir / "sample_text.txt").write_text(sample_text)


@pytest.fixture(scope="session")
def mock_data_orig(tmp_path_factory):
    orig_name = tmp_path_factory.mktemp(tools.get_output_name())
    create_data_dir(orig_name)

    asyncio.set_event_loop(asyncio.new_event_loop())
    make_pretend_repo(orig_name)
    tools.remove_w_perms(orig_name)
    return orig_name


@pytest.fixture(scope="session")
def get_mock_data(tmp_path_factory, mock_data_orig):
    tmpdirs = []

    def _get_mock_data():

        tmpdir = tmp_path_factory.mktemp(tools.get_output_name())

        create_data_dir(tmpdir)
        tmpdirs.append(tmpdir)

        outdir = get_outdir(tmpdir)
        data = tools.get_command_info(outdir)
        cmd_exe_vals = ["user", "host", "cmd", "workdir"]
        for k, v in data.items():
            if "/" in v:
                data[k] = Path(v)

        data["create_sample_output"] = False
        data["save_sample_output"] = False

        for k in cmd_exe_vals:
            data.pop(k, None)

        DataClass = attr.make_class(
            data["test_name"] + "_data", [k for k in data.keys()], slots=True
        )
        data = DataClass(*[v for v in data.values()])
        return tmpdir, data

    yield _get_mock_data

    for tmp_data_dir in tmpdirs:
        shutil.rmtree(tmp_data_dir)


# def test_rewrite_paths_for_cleaner_diffs(get_mock_data,mock_data_orig):
#     output_mock, data = get_mock_data()


# @pytest.fixture(scope="function")
# def output_mock(output_mock_orig):
#     outdir = output_mock_orig.with_name(tools.get_current_test_name() + "_0")
#     while outdir.exists():
#         parts = outdir.name.rsplit("_", 1)
#         new = int(parts[1]) + 1
#         outdir = outdir.parent / f"{parts[0]}_{new}"

#     shutil.copytree(output_mock_orig, outdir, symlinks=True)
#     return outdir


def test_diffs_detected(get_mock_data, mock_data_orig):

    output_mock, data = get_mock_data()
    differ = tools.OutputDiffer(data, "echo `pwd`")
    differ.get_file_list()
    differ._text_file_patterns = [".txt"]
    differ.assert_all_files_equal()


# @pytest.fixture()
# def comparison_dir_factory():


# @pytest.fixture()
# def mock_test_data_dir():
#     pass


# @pytest.fixture()
# def mock_test_outdir_matching():
#     pass


# @pytest.fixture()
# def mock_test_outdir_diffs():
#     pass


# @pytest.fixture()
# def mock_test_outdir_should_fail():
#     pass


# @pytest.fixture()
# def mock_test_outdir_does_not_exist():
#     pass


# from pathlib import Path

# from john_run_algo.utils import data_management


# def fake_storage():

#     print("executing fake_storage")
#     return Path("goodbye")


# def test_check_data_store(mocker):

#     mocker.patch(
#         "john_run_algo.utils.data_management.get_storage_dir",
#         return_value=Path("return_value"),
#     )
#     data_management.check_data_store("m_end", ["symbol"])
#     assert False
