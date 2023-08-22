import os
import stat
from argparse import ArgumentParser

from textwrap import dedent
from ecl.util.geometry import Surface
from ert.field_utils.grdecl_io import export_grdecl

import numpy as np
import xtgeo

from ert.__main__ import ert_parser
from ert.cli import ENSEMBLE_SMOOTHER_MODE
from ert.cli.main import run_cli


def test_surface_mem(tmpdir):
    with tmpdir.as_cwd():
        config = dedent(
            """
            NUM_REALIZATIONS 5
            OBS_CONFIG observations.txt
            SURFACE SURFACE_1  OUTPUT_FILE:surface1_out.irap   INIT_FILES:surface1_%d.irap   BASE_SURFACE:surface1_base.irap
            SURFACE SURFACE_2  OUTPUT_FILE:surface2_out.irap   INIT_FILES:surface2_%d.irap   BASE_SURFACE:surface2_base.irap
            SURFACE SURFACE_3  OUTPUT_FILE:surface3_out.irap   INIT_FILES:surface3_%d.irap   BASE_SURFACE:surface3_base.irap
            SURFACE SURFACE_4  OUTPUT_FILE:surface4_out.irap   INIT_FILES:surface4_%d.irap   BASE_SURFACE:surface4_base.irap
            GRID MY_EGRID.EGRID
            GEN_DATA MY_RESPONSE RESULT_FILE:gen_data_%d.out REPORT_STEPS:0 INPUT_FORMAT:ASCII
            INSTALL_JOB poly_eval POLY_EVAL
            SIMULATION_JOB poly_eval
        """  # pylint: disable=line-too-long  # noqa: E501
        )
        with open("config.ert", "w", encoding="utf-8") as fh:
            fh.writelines(config)

        num_realizations = 5
        ncol = 1000
        nrow = 9000
        nlay = 1
        grid = xtgeo.create_box_grid(dimension=(ncol, nrow, nlay))
        grid.to_file("MY_EGRID.EGRID", "egrid")

        surface = Surface(nx=ncol, ny=nrow, xinc=1, yinc=1, xstart=1, ystart=1, angle=0)
        surface.write("surface1_base.irap")
        surface.write("surface2_base.irap")
        surface.write("surface3_base.irap")
        surface.write("surface4_base.irap")

        for i in range(num_realizations):
            surface.write(f"surface1_{i}.irap")
            surface.write(f"surface2_{i}.irap")
            surface.write(f"surface3_{i}.irap")
            surface.write(f"surface4_{i}.irap")

        with open("template.txt", mode="w", encoding="utf-8") as fh:
            fh.writelines("MY_KEYWORD <MY_KEYWORD>")
        with open("prior.txt", mode="w", encoding="utf-8") as fh:
            fh.writelines("MY_KEYWORD NORMAL 0 1")
        with open("custom_param0.txt", mode="w", encoding="utf-8") as fh:
            fh.writelines("MY_KEYWORD 1.31")

        with open("config.ert", "w", encoding="utf-8") as fh:
            fh.writelines(config)

        grid = xtgeo.create_box_grid(dimension=(10, 10, 1))
        grid.to_file("MY_EGRID.EGRID", "egrid")

        with open("observations.txt", "w", encoding="utf-8") as fout:
            fout.write(
                dedent(
                    """
            GENERAL_OBSERVATION MY_OBS {
                DATA       = MY_RESPONSE;
                INDEX_LIST = 0,2,4,6,8;
                RESTART    = 0;
                OBS_FILE   = obs.txt;
            };"""
                )
            )

        with open("obs.txt", "w", encoding="utf-8") as fobs:
            fobs.write(
                dedent(
                    """
            2.1457049781272213 0.6
            8.769219841380755 1.4
            12.388014786122742 3.0
            25.600464531354252 5.4
            42.35204755970952 8.6"""
                )
            )

        with open("forward_model", "w", encoding="utf-8") as f:
            f.write(
                dedent(
                    """#!/usr/bin/env python
import xtgeo
import numpy as np
import os
if __name__ == "__main__":
    if not os.path.exists("my_param.grdecl"):
        values = np.random.standard_normal(4*4)
        with open("my_param.grdecl", "w") as fout:
            fout.write("MY_PARAM\\n")
            fout.write(" ".join([str(val) for val in values]) + " /\\n")
    with open("my_param.grdecl", "r") as fin:
        for line_nr, line in enumerate(fin):
            if line_nr == 1:
                a, b, c, *_ = line.split()
    output = [float(a) * x**2 + float(b) * x + float(c) for x in range(10)]
    with open("gen_data_0.out", "w", encoding="utf-8") as f:
        f.write("\\n".join(map(str, output)))
        """
                )
            )
        os.chmod(
            "forward_model",
            os.stat("forward_model").st_mode
            | stat.S_IXUSR
            | stat.S_IXGRP
            | stat.S_IXOTH,
        )
        with open("POLY_EVAL", "w", encoding="utf-8") as fout:
            fout.write("EXECUTABLE forward_model")

        parser = ArgumentParser(prog="test_main")
        parsed = ert_parser(
            parser,
            [
                ENSEMBLE_SMOOTHER_MODE,
                "--current-case",
                "prior",
                "--target-case",
                "smoother_update",
                "config.ert",
                "--port-range",
                "1024-65535",
            ],
        )

        run_cli(parsed)


def test_field_mem(tmpdir):
    with tmpdir.as_cwd():
        config = dedent(
            """
            NUM_REALIZATIONS 5
            OBS_CONFIG observations.txt
            FIELD FIELD1 PARAMETER field_1.grdecl INIT_FILES:field_1_%d.grdecl
            FIELD FIELD2 PARAMETER field_2.grdecl INIT_FILES:field_2_%d.grdecl
            FIELD FIELD3 PARAMETER field_3.grdecl INIT_FILES:field_3_%d.grdecl
            FIELD FIELD4 PARAMETER field_4.grdecl INIT_FILES:field_4_%d.grdecl
            GRID MY_EGRID.EGRID
            GEN_DATA MY_RESPONSE RESULT_FILE:gen_data_%d.out REPORT_STEPS:0 INPUT_FORMAT:ASCII
            INSTALL_JOB poly_eval POLY_EVAL
            SIMULATION_JOB poly_eval
        """  # pylint: disable=line-too-long  # noqa: E501
        )
        with open("config.ert", "w", encoding="utf-8") as fh:
            fh.writelines(config)

        num_realizations = 5
        ncol = 1000
        nrow = 9000
        nlay = 1
        grid = xtgeo.create_box_grid(dimension=(ncol, nrow, nlay))
        grid.to_file("MY_EGRID.EGRID", "egrid")

        # Generate fields of size about 100M
        from ert.field_utils.grdecl_io import open_grdecl, export_grdecl

        field = np.random.rand(ncol, nrow)
        for i in range(num_realizations):
            export_grdecl(field, f"field_1_{i}.grdecl", "FIELD1", False)
            export_grdecl(field, f"field_2_{i}.grdecl", "FIELD2", False)
            export_grdecl(field, f"field_3_{i}.grdecl", "FIELD3", False)
            export_grdecl(field, f"field_4_{i}.grdecl", "FIELD4", False)

        with open("observations.txt", "w", encoding="utf-8") as fout:
            fout.write(
                dedent(
                    """
            GENERAL_OBSERVATION MY_OBS {
                DATA       = MY_RESPONSE;
                INDEX_LIST = 0,2,4,6,8;
                RESTART    = 0;
                OBS_FILE   = obs.txt;
            };"""
                )
            )

        with open("obs.txt", "w", encoding="utf-8") as fobs:
            fobs.write(
                dedent(
                    """
            2.1457049781272213 0.6
            8.769219841380755 1.4
            12.388014786122742 3.0
            25.600464531354252 5.4
            42.35204755970952 8.6"""
                )
            )

        with open("forward_model", "w", encoding="utf-8") as f:
            f.write(
                dedent(
                    """#!/usr/bin/env python
import xtgeo
import numpy as np
import os
if __name__ == "__main__":
    if not os.path.exists("my_param.grdecl"):
        values = np.random.standard_normal(4*4)
        with open("my_param.grdecl", "w") as fout:
            fout.write("MY_PARAM\\n")
            fout.write(" ".join([str(val) for val in values]) + " /\\n")
    with open("my_param.grdecl", "r") as fin:
        for line_nr, line in enumerate(fin):
            if line_nr == 1:
                a, b, c, *_ = line.split()
    output = [float(a) * x**2 + float(b) * x + float(c) for x in range(10)]
    with open("gen_data_0.out", "w", encoding="utf-8") as f:
        f.write("\\n".join(map(str, output)))
        """
                )
            )
        os.chmod(
            "forward_model",
            os.stat("forward_model").st_mode
            | stat.S_IXUSR
            | stat.S_IXGRP
            | stat.S_IXOTH,
        )
        with open("POLY_EVAL", "w", encoding="utf-8") as fout:
            fout.write("EXECUTABLE forward_model")

        parser = ArgumentParser(prog="test_main")
        parsed = ert_parser(
            parser,
            [
                ENSEMBLE_SMOOTHER_MODE,
                "--current-case",
                "prior",
                "--target-case",
                "smoother_update",
                "config.ert",
                "--port-range",
                "1024-65535",
            ],
        )

        run_cli(parsed)





def test_field_mem_fast(tmpdir):
    with tmpdir.as_cwd():
        config = dedent(
            """
            NUM_REALIZATIONS 5
            OBS_CONFIG observations.txt
            FIELD FIELD1 PARAMETER field_1.grdecl INIT_FILES:field_1_%d.grdecl
            FIELD FIELD2 PARAMETER field_2.grdecl INIT_FILES:field_2_%d.grdecl
            GRID MY_EGRID.EGRID
            GEN_DATA MY_RESPONSE RESULT_FILE:gen_data_%d.out REPORT_STEPS:0 INPUT_FORMAT:ASCII
            INSTALL_JOB poly_eval POLY_EVAL
            SIMULATION_JOB poly_eval
        """  # pylint: disable=line-too-long  # noqa: E501
        )
        with open("config.ert", "w", encoding="utf-8") as fh:
            fh.writelines(config)

        num_realizations = 5
        ncol = 100
        nrow = 900
        nlay = 1
        grid = xtgeo.create_box_grid(dimension=(ncol, nrow, nlay))
        grid.to_file("MY_EGRID.EGRID", "egrid")

        field = np.random.rand(ncol, nrow)
        for i in range(num_realizations):
            export_grdecl(field, f"field_1_{i}.grdecl", "FIELD1", False)
            export_grdecl(field, f"field_2_{i}.grdecl", "FIELD2", False)

        with open("observations.txt", "w", encoding="utf-8") as fout:
            fout.write(
                dedent(
                    """
            GENERAL_OBSERVATION MY_OBS {
                DATA       = MY_RESPONSE;
                INDEX_LIST = 0,2,4,6,8;
                RESTART    = 0;
                OBS_FILE   = obs.txt;
            };"""
                )
            )

        with open("obs.txt", "w", encoding="utf-8") as fobs:
            fobs.write(
                dedent(
                    """
            2.1457049781272213 0.6
            8.769219841380755 1.4
            12.388014786122742 3.0
            25.600464531354252 5.4
            42.35204755970952 8.6"""
                )
            )

        with open("forward_model", "w", encoding="utf-8") as f:
            f.write(
                dedent(
                    """#!/usr/bin/env python
import xtgeo
import numpy as np
import os
if __name__ == "__main__":
    if not os.path.exists("my_param.grdecl"):
        values = np.random.standard_normal(4*4)
        with open("my_param.grdecl", "w") as fout:
            fout.write("MY_PARAM\\n")
            fout.write(" ".join([str(val) for val in values]) + " /\\n")
    with open("my_param.grdecl", "r") as fin:
        for line_nr, line in enumerate(fin):
            if line_nr == 1:
                a, b, c, *_ = line.split()
    output = [float(a) * x**2 + float(b) * x + float(c) for x in range(10)]
    with open("gen_data_0.out", "w", encoding="utf-8") as f:
        f.write("\\n".join(map(str, output)))
        """
                )
            )
        os.chmod(
            "forward_model",
            os.stat("forward_model").st_mode
            | stat.S_IXUSR
            | stat.S_IXGRP
            | stat.S_IXOTH,
        )
        with open("POLY_EVAL", "w", encoding="utf-8") as fout:
            fout.write("EXECUTABLE forward_model")

        parser = ArgumentParser(prog="test_main")
        parsed = ert_parser(
            parser,
            [
                ENSEMBLE_SMOOTHER_MODE,
                "--current-case",
                "prior",
                "--target-case",
                "smoother_update",
                "config.ert",
                "--port-range",
                "1024-65535",
            ],
        )

        run_cli(parsed)
