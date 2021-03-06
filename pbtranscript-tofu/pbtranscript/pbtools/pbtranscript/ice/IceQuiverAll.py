#!/usr/bin/env python
###############################################################################
# Copyright (c) 2011-2013, Pacific Biosciences of California, Inc.
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Pacific Biosciences nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
# THIS LICENSE.  THIS SOFTWARE IS PROVIDED BY PACIFIC BIOSCIENCES AND ITS
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
# NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PACIFIC BIOSCIENCES OR
# ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
###############################################################################

"""
Description:
    ice_quiver.py all

    Assumption:
      * Iterative clustering (IceIterative) is done, fl reads are
        assigned to isoforms and saved to a pickle (i.e., final.pickle).
      * IcePartialAll is done, all non-full-length reads are assigned to
        unpolished isoforms, and saved to a pickle (i.e., nfl_all_pickle_fn).
      * ice_make_fasta_fofn is done, fasta_fofn is created by extracting
        subreads from bas.h5 files.

    Process:
        After assigning all non-full-length reads to unpolished consensus
        isoforms, call quiver to polish these isoforms and output high QV and
        low QV isoforms.

    Input:
        Positional:
            root_dir, an output directory for running pbtranscript cluster.
        Non-positional, but required:
            bas_fofn, fofn of input bas/bax.h5 files
            fasta_fofn, fofn of fasta files of subreads extracted from
            bas/baxh5 files

    Output:
        Polished consensus isoforms and HQ/LQ polished consensus isoforms
        in fasta/q, write a report and a summary.

    Hierarchy:
        pbtranscript = IceIterative

        pbtranscript --quiver = IceIterative + \
                                ice_polish.py

        ice_polish.py =  ice_make_fasta_fofn.py + \
                         ice_partial.py all + \
                         ice_quiver.py all

        ice_partial.py all = ice_partial.py split + \
                             ice_partial.py i + \
                             ice_partial.py merge

        ice_quiver.py all = ice_quiver.py i + \
                            ice_quiver.py merge + \
                            ice_quiver.py postprocess

    Example:
        ice_quiver.py all root_dir --bas_fofn=bas_fofn --fasta_fofn=fasta_fofn
"""

from pbtools.pbtranscript.PBTranscriptOptions import \
        add_cluster_root_dir_as_positional_argument, \
        add_fofn_arguments, add_cluster_summary_report_arguments, \
        add_ice_post_quiver_hq_lq_arguments, \
        add_sge_arguments
from pbtools.pbtranscript.ice.IceQuiver import IceQuiver
from pbtools.pbtranscript.ice.IceQuiverPostprocess import IceQuiverPostprocess

class IceQuiverAll(object):

    """IceQuiverAll."""

    desc = "After assigning all non-full-length reads to unpolished " + \
           "consensus isoforms (e.g., 'ice_partial.py all' is done), " + \
           "call quiver to polish these isoforms, then output " + \
           "high QV and low QV isoforms."

    prog = "ice_quiver.py all "

    def __init__(self, root_dir, bas_fofn, fasta_fofn, sge_opts, ipq_opts,
                 report_fn=None, summary_fn=None, prog_name=None):
        prog_name = prog_name if prog_name is not None else "IceQuiverAll"
        self.root_dir = root_dir
        self.bas_fofn = bas_fofn
        self.fasta_fofn = fasta_fofn
        self.report_fn = report_fn
        self.summary_fn = summary_fn
        self.sge_opts = sge_opts
        self.ipq_opts = ipq_opts

    def cmd_str(self):
        """Return a cmd string. (ice_quiver.py all)."""
        return self._cmd_str(root_dir=self.root_dir, bas_fofn=self.bas_fofn,
                             fasta_fofn=self.fasta_fofn, sge_opts=self.sge_opts,
                             ipq_opts=self.ipq_opts, report_fn=self.report_fn,
                             summary_fn=self.summary_fn)


    def _cmd_str(self, root_dir, bas_fofn, fasta_fofn, sge_opts, ipq_opts,
                 report_fn, summary_fn):
        """Return a cmd string. (ice_quiver.py all)."""
        cmd = self.prog + \
              "{d} ".format(d=root_dir) + \
              "--bas_fofn={f} ".format(f=bas_fofn) + \
              "--fasta_fofn={f} ".format(f=fasta_fofn)
        if report_fn is not None:
            cmd += "--report={f} ".format(f=report_fn)
        if summary_fn is not None:
            cmd += "--summary={f} ".format(f=summary_fn)
        cmd += sge_opts.cmd_str(show_blasr_nproc=True, show_quiver_nproc=True)
        cmd += ipq_opts.cmd_str()
        return cmd

    def run(self):
        """Run"""
        iceq = IceQuiver(root_dir=self.root_dir, bas_fofn=self.bas_fofn,
                         fasta_fofn=self.fasta_fofn, sge_opts=self.sge_opts)
        iceq.validate_inputs()
        iceq.run()

        icepq = IceQuiverPostprocess(root_dir=self.root_dir,
                                     use_sge=self.sge_opts.use_sge,
                                     quit_if_not_done=False,
                                     ipq_opts=self.ipq_opts)
        icepq.run()


def add_ice_quiver_all_arguments(parser):
    """Add arguments for IceQuiverAll, including arguments for IceQuiver and
    IceQuiverPostprocess."""
    parser = add_cluster_root_dir_as_positional_argument(parser)
    parser = add_fofn_arguments(parser, bas_fofn=True, fasta_fofn=True)
    parser = add_cluster_summary_report_arguments(parser)
    parser = add_ice_post_quiver_hq_lq_arguments(parser)
    parser = add_sge_arguments(parser, quiver_nproc=True, blasr_nproc=True)
    return parser
