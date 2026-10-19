#!/usr/bin/env python3
"""
Writes the toy SDDPBlock instances on which the simulation that the
InvestmentFunction runs at a fixed design is compared with the one of
sddp_solver, each once as an SDDPBlock alone (sddp_<variant>.nc4) and once
as the inner Block of an InvestmentBlock (ib_sddp_<variant>.nc4).

Three stages of 4 instants each, two scenarios, one bus. The units of the
UCBlock of every stage are a reservoir (a HydroSystemUnitBlock whose only
arc has no flow and no inflow, so that the state of the SDDPBlock, its
volume, stays at 50 and the cost-to-go is the same at every state, i.e.,
the decisions of a simulation do not depend on the cuts of the training), a
thermal unit (0 to 100, 10 per MWh, 50 per instant on, 100 per start-up,
start-up and shut-down limits and ramps as wide as the maximum power, off
since 10 instants before the horizon of every stage) and a slack unit at
20 per MWh. The demand is 0 everywhere except at one instant of the first
stage, where it is 100 in the first scenario and 130 in the second, so
that the thermal unit is fully on there also in the continuous relaxation
(which the backward pass of the training solves) and the training converges
with the relaxation and the exact problem giving the same value; the design
is the thermal unit, fixed at its installed quantity 1, at no cost, hence
the value of the InvestmentFunction is the mean of the costs of the two
simulations.

  minup3    the peak is at the last instant of the first stage, and the unit
            has MinUpTime 3: it starts up there, and it has to stay on for
            the first 2 instants of the second stage, which costs 2 * 50
            there; the mean is (1150 + 1750) / 2 + 100 = 1550

  minup1    the same with MinUpTime 1: the unit is on before the horizon of
            the second stage and switches off at its first instant, which
            costs nothing, so that being off or on before the horizon gives
            the second stage the same cost; the mean is
            (1150 + 1750) / 2 = 1450

  noswitch  the peak is at the first instant of the first stage, and the
            unit has MinUpTime and MinDownTime 2: it is on at the first two
            instants and off at the last two, so it is off before the second
            stage as the data of the second stage say; the mean is
            (1200 + 1800) / 2 = 1500

usage: gen_sddp_initud.py <output directory>

The instances are written by ncgen from their CDL, and the reference values
(the mean of the costs of the simulations of the two scenarios) are printed
in the form the battery reads them.
"""

import subprocess
import sys
from pathlib import Path

STAGES = 3
T = 4
PEAK = [100, 130]
REF = {"minup3": 1550, "minup1": 1450, "noswitch": 1500}
# per variant: the instant of the peak in the first stage, MinUpTime and
# MinDownTime of the thermal unit
VARIANT = {"minup3": (T - 1, 3, 2), "minup1": (T - 1, 1, 1),
           "noswitch": (0, 2, 2)}


def vals(v):
    return ", ".join(repr(float(x)) for x in v)


def demand(variant, scenario, stage):
    d = [0] * T
    if stage == 0:
        d[VARIANT[variant][0]] = PEAK[scenario]
    return d


def ucblock(variant, stage):
    _, minup, mindown = VARIANT[variant]
    return [
        "group: Block {", "dimensions:", " TimeHorizon = %d ;" % T,
        " NumberUnits = 3 ;", "variables:",
        " double ActivePowerDemand(TimeHorizon) ;", ':id = "0" ;',
        ':type = "UCBlock" ;', "data:",
        " ActivePowerDemand = %s ;" % vals(demand(variant, 0, stage)),
        # the reservoir, whose volume is the state
        "group: UnitBlock_0 {", "dimensions:", " NumberHydroUnits = 1 ;",
        ':type = "HydroSystemUnitBlock" ;',
        "group: HydroUnitBlock_0 {", "dimensions:",
        " NumberReservoirs = 1 ;", " NumberArcs = 1 ;", "variables:",
        " uint StartArc(NumberArcs) ;", " uint EndArc(NumberArcs) ;",
        " double MinVolumetric(NumberReservoirs) ;",
        " double MaxVolumetric(NumberReservoirs) ;",
        " double Inflows(NumberReservoirs, TimeHorizon) ;",
        " double MinPower(NumberArcs) ;", " double MaxPower(NumberArcs) ;",
        " double MinFlow(NumberArcs) ;", " double MaxFlow(NumberArcs) ;",
        " uint NumberPieces(NumberArcs) ;",
        " double LinearTerm(NumberArcs) ;",
        " double ConstantTerm(NumberArcs) ;",
        " double InitialVolumetric(NumberReservoirs) ;",
        " double InitialFlowRate(NumberArcs) ;",
        ':type = "HydroUnitBlock" ;', "data:", " StartArc = 0 ;",
        " EndArc = 1 ;", " MinVolumetric = 0 ;", " MaxVolumetric = 100 ;",
        " Inflows = %s ;" % vals([0] * T), " MinPower = 0 ;",
        " MaxPower = 0 ;", " MinFlow = 0 ;", " MaxFlow = 0 ;",
        " NumberPieces = 1 ;", " LinearTerm = 1 ;", " ConstantTerm = 0 ;",
        " InitialVolumetric = 50 ;", " InitialFlowRate = 0 ;", "}",
        "group: PolyhedralFunctionBlock {", "dimensions:",
        " PolyFunction_NumVar = 1 ;", ':type = "PolyhedralFunctionBlock" ;',
        "}", "}",
        # the thermal unit, off since 10 instants at the start of every stage
        "group: UnitBlock_1 {", "variables:", " double MinPower ;",
        " double MaxPower ;", " double DeltaRampUp ;",
        " double DeltaRampDown ;", " double StartUpLimit ;",
        " double ShutDownLimit ;", " double StartUpCost ;",
        " double LinearTerm ;", " double ConstTerm ;",
        " double InitialPower ;", " int InitUpDownTime ;",
        " uint MinUpTime ;", " uint MinDownTime ;",
        ':type = "ThermalUnitBlock" ;', "data:", " MinPower = 0 ;",
        " MaxPower = 100 ;", " DeltaRampUp = 100 ;",
        " DeltaRampDown = 100 ;", " StartUpLimit = 100 ;",
        " ShutDownLimit = 100 ;", " StartUpCost = 100 ;",
        " LinearTerm = 10 ;", " ConstTerm = 50 ;", " InitialPower = 0 ;",
        " InitUpDownTime = -10 ;", " MinUpTime = %d ;" % minup,
        " MinDownTime = %d ;" % mindown, "}",
        # the slack unit
        "group: UnitBlock_2 {", "variables:", " double MaxPower ;",
        " double ActivePowerCost ;", ':type = "SlackUnitBlock" ;', "data:",
        " MaxPower = 1000 ;", " ActivePowerCost = 20 ;", "}", "}"]


def stage(variant, s):
    # the data of the scenario mapped onto the stage: the demand (4 values)
    # into the UCBlock, the inflows (4 values) into the reservoir, and the
    # first inflow into the constant of the BendersBFunction, the row that
    # gives the volume at the first instant from the state
    return [
        "group: StochasticBlock_%d {" % s, "dimensions:",
        " NumberDataMappings = 3 ;", " SetSizeSize = 6 ;",
        " SetElementsSize = 12 ;", "variables:",
        " char DataType(NumberDataMappings) ;",
        " uint SetSize(SetSizeSize) ;", " uint SetElements(SetElementsSize) ;",
        " string FunctionName(NumberDataMappings) ;",
        " char Caller(NumberDataMappings) ;", ':type = "StochasticBlock" ;',
        "data:", ' DataType = "DDD" ;', " SetSize = 4, 0, 0, 0, 1, 1 ;",
        " SetElements = 0, 1, 2, 3, 0, 4, 4, 8, 0, 4, 4, 0 ;",
        ' FunctionName = "UCBlock::set_active_power_demand",'
        ' "HydroUnitBlock::set_inflow",'
        ' "BendersBFunction::modify_constants" ;',
        ' Caller = "BBF" ;',
        "group: AbstractPath {", "dimensions:", " PathDim = 3 ;",
        " TotalLength = 7 ;", "variables:", " uint PathStart(PathDim) ;",
        " char PathNodeTypes(TotalLength) ;",
        " uint PathGroupIndices(TotalLength) ;", "data:",
        " PathStart = 0, 2, 6 ;", ' PathNodeTypes = "OBOBBBO" ;',
        " PathGroupIndices = _, 0, _, 0, 0, 0, _ ;", "}",
        "group: Block {", "dimensions:", " NumVar = 1 ;",
        ':type = "BendersBlock" ;',
        "group: BendersBFunction {", "dimensions:", " NumVar = 1 ;",
        " NumRow = 1 ;", " NumNonzero = 1 ;"] + ucblock(variant, s) + [
        "group: AbstractPath {", "dimensions:", " PathDim = 1 ;",
        " TotalLength = 3 ;", "variables:",
        " char PathNodeTypes(TotalLength) ;",
        " uint PathGroupIndices(TotalLength) ;",
        " uint PathElementIndices(TotalLength) ;",
        " uint PathStart(PathDim) ;",
        "data:", ' PathNodeTypes = "BBC" ;', " PathGroupIndices = 0, 0, 0 ;",
        " PathElementIndices = _, _, 0 ;", " PathStart = 0 ;", "}", "}", "}",
        "}"]


def sddpblock(variant):
    scen = []
    for k in range(len(PEAK)):
        row = []
        for s in range(STAGES):
            d = [0] * T
            if s == 0:
                d[VARIANT[variant][0]] = PEAK[k]
            row += d + [0] * T
        scen.append(vals(row))
    o = ["dimensions:", " NumPolyhedralFunctionsPerStage = 1 ;",
         " TimeHorizon = %d ;" % STAGES, " NumberScenarios = %d ;" % len(PEAK),
         " SubScenarioSize = %d ;" % (2 * T),
         " ScenarioSize = %d ;" % (2 * T * STAGES),
         " NumberRandomDataGroups = 2 ;",
         " AdmissibleStateSize = %d ;" % STAGES, " InitialStateSize = 1 ;",
         "variables:", " uint SizeRandomDataGroups(NumberRandomDataGroups) ;",
         " double Scenarios(NumberScenarios, ScenarioSize) ;",
         " uint StateSize ;", " double AdmissibleState(AdmissibleStateSize) ;",
         " double InitialState(InitialStateSize) ;", ':type = "SDDPBlock" ;',
         "data:", " SizeRandomDataGroups = %d, %d ;" % (T, T),
         " Scenarios = %s ;" % ", ".join(scen), " StateSize = 1 ;",
         " AdmissibleState = %s ;" % vals([50] * STAGES),
         " InitialState = 50 ;"]
    for s in range(STAGES):
        o += stage(variant, s)
    # the path from the BendersBlock of a stage to its PolyhedralFunctionBlock
    return o + [
        "group: AbstractPath {", "dimensions:", " PathDim = 1 ;",
        " TotalLength = 4 ;", "variables:",
        " char PathNodeTypes(TotalLength) ;",
        " uint PathGroupIndices(TotalLength) ;", " uint PathStart(PathDim) ;",
        "data:", ' PathNodeTypes = "OBBB" ;',
        " PathGroupIndices = _, 0, 0, 1 ;", " PathStart = 0 ;", "}"]


def sddp_file(variant):
    o = ["netcdf sddp {", ":SMS++_file_type = 1 ;", "group: SDDPBlock {"]
    return "\n".join(o + sddpblock(variant) + ["}", "}"]) + "\n"


def ib_file(variant):
    o = ["netcdf ib_sddp {", ":SMS++_file_type = 1 ;", "group: Block_0 {",
         "dimensions:", " NumAssets = 1 ;", "variables:",
         " double LowerBound ;", " double UpperBound ;", " double Cost ;",
         " double InstalledQuantity ;", " uint Assets(NumAssets) ;",
         " byte AssetType ;", ':type = "InvestmentBlock" ;', "data:",
         " LowerBound = 1 ;", " UpperBound = 1 ;", " Cost = 0 ;",
         " InstalledQuantity = 1 ;", " Assets = 1 ;", " AssetType = 0 ;",
         "group: InnerBlock {"]
    return "\n".join(o + sddpblock(variant) + ["}", "}", "}"]) + "\n"


def write(out, name, text):
    c = out / (name + ".cdl")
    c.write_text(text)
    subprocess.run(["ncgen", "-k", "nc4", "-o", str(out / (name + ".nc4")),
                    str(c)], check=True)
    c.unlink()


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: gen_sddp_initud.py <output directory>")
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    for variant in VARIANT:
        write(out, "sddp_" + variant, sddp_file(variant))
        write(out, "ib_sddp_" + variant, ib_file(variant))
        print("REF_OBJ[%s]=%s" % (variant, REF[variant]))


if __name__ == "__main__":
    main()
