#!/usr/bin/env python
#############################################################################
#
#   Copyright (C) 2013-2016 PX4 Development Team. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name PX4 nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
#############################################################################

"""
px_generate_mixers.py
Generates c/cpp header/source files for multirotor mixers
from geometry descriptions files (.toml format)
"""

try:
    import toml
    import numpy as np
except ImportError as e:
    print("python import error: ", e)
    print('''
Required python packages not installed.

On a Debian/Ubuntu system please run:

  sudo apt-get install python-toml python-numpy

On MacOS please run:
  sudo pip install numpy toml

On Windows please run:
  easy_install numpy toml
''')
    exit(1)

__author__ = "Julien Lecoeur"
__copyright__ = "Copyright (C) 2013-2017 PX4 Development Team."
__license__ = "BSD"
__email__ = "julien.lecoeur@gmail.com"

def parse_geom_toml(filename):
    '''
    Parses toml geometry file and returns a dictionary with curated list of rotors
    '''

    import toml

    # Load toml file
    d = toml.load(filename)

    # Check default rotor config
    if 'rotor_default' in d:
        default = d['rotor_default']
    else:
        default = {}

    # Convert rotors
    rotor_list = []
    if 'rotors' in d:
        for r in d['rotors']:
            # Make sure all fields are defined, fill missing with default
            for field in ['name', 'position', 'axis', 'direction', 'Ct', 'Cm']:
                if field not in r:
                    if field in default:
                        r[field] = default[field]
                    else:
                        print(filename + ': Error, unspecified field ' + field + ' for rotor ' + r['name'])

            # Check fields
            r['direction'] = r['direction'].upper()
            if r['direction'] not in ['CW', 'CCW']:
                print(filename + ': Error, invalid direction value "' + r['direction'] + '" for rotor ' + r['name'])


            # Add rotor to list
            rotor_list.append(r)

    # Clean dictionary
    geom = {'info': d['info'],
           'rotors': rotor_list}

    return geom

def torque_matrix(center, axis, dirs, Ct, Cm):
    '''
    Compute torque generated by rotors
    '''
    # normalize rotor axis
    ax     = axis / np.linalg.norm(axis, axis=1)[:,np.newaxis]
    torque = Ct * np.cross(center, ax) - Cm * ax * dirs
    return torque

def geom_to_torque_matrix(geom):
    '''
    Compute torque matrix Am and Bm from geometry dictionnary
    Am is a 3xN matrix where N is the number of rotors
    Each column is the torque generated by one rotor
    '''
    Am = torque_matrix(center=np.array([rotor['position'] for rotor in geom['rotors']]),
                       axis=np.array([rotor['axis'] for rotor in geom['rotors']]),
                       dirs=np.array([[1.0 if rotor['direction'] == 'CCW' else -1.0]
                                       for rotor in geom['rotors']]),
                       Ct=np.array([[rotor['Ct']] for rotor in geom['rotors']]),
                       Cm=np.array([[rotor['Cm']] for rotor in geom['rotors']])).T
    return Am

def thrust_matrix(axis, Ct):
    '''
    Compute thrust generated by rotors
    '''
    # Normalize rotor axis
    ax     = axis / np.linalg.norm(axis, axis=1)[:,np.newaxis]
    thrust = Ct * ax
    return thrust

def geom_to_thrust_matrix(geom):
    '''
    Compute thrust matrix At from geometry dictionnary
    At is a 3xN matrix where N is the number of rotors
    Each column is the thrust generated by one rotor
    '''
    At = thrust_matrix(axis=np.array([rotor['axis'] for rotor in geom['rotors']]),
                       Ct=np.array([[rotor['Ct']] for rotor in geom['rotors']])).T

    return At

def geom_to_mix(geom):
    '''
    Compute combined torque & thrust matrix A and mix matrix B from geometry dictionnary

    A is a 6xN matrix where N is the number of rotors
    Each column is the torque and thrust generated by one rotor

    B is a Nx6 matrix where N is the number of rotors
    Each column is the command to apply to the servos to get
    roll torque, pitch torque, yaw torque, x thrust, y thrust, z thrust
    '''
    # Combined torque & thrust matrix
    At = geom_to_thrust_matrix(geom)
    Am = geom_to_torque_matrix(geom)
    A = np.vstack([Am, At])

    # Mix matrix computed as pseudoinverse of A
    B = np.linalg.pinv(A)

    return A, B

def normalize_mix_px4(B):
    '''
    Normalize mix for PX4
    This is for compatibility only and should ideally not be used
    '''
    B_norm = np.linalg.norm(B,axis=0)
    B_max  = np.abs(B).max(axis=0)

    # Same scale on roll and pitch
    B_norm[0] = max(B_norm[0], B_norm[1]) / np.sqrt(B.shape[0] / 2.0)
    B_norm[1] = B_norm[0]

    # Scale yaw separately
    B_norm[2] = B_max[2]

    # Same scale on x, y
    B_norm[3] = max(B_max[3], B_max[4])
    B_norm[4] = B_norm[3]

    # Scale z thrust separately
    B_norm[5] = B_max[5]

    # Normalize
    B_norm[np.abs(B_norm)<1e-3] = 1
    B_px = (B / B_norm)

    return B_px

def generate_mixer_multirotor_header(geom_list, use_normalized_mix=False, use_6dof=False):
    '''
    Generate C header file with same format as multi_tables.py
    TODO: rewrite using templates (see generation of uORB headers)
    '''
    from io import StringIO
    buf = StringIO()

    # Print Header
    buf.write(u"/*\n")
    buf.write(u"* This file is automatically generated by px_generate_mixers.py - do not edit.\n")
    buf.write(u"*/\n")
    buf.write(u"\n")
    buf.write(u"#ifndef _MIXER_MULTI_TABLES\n")
    buf.write(u"#define _MIXER_MULTI_TABLES\n")
    buf.write(u"\n")

    # Print enum
    buf.write(u"enum class MultirotorGeometry : MultirotorGeometryUnderlyingType {\n")
    for i, geom in enumerate(geom_list):
        buf.write(u"\t{} = {},\n".format(geom['info']['name'].upper(), i))
    buf.write(u"\n\tMAX_GEOMETRY\n")
    buf.write(u"}; // enum class MultirotorGeometry\n\n")

    # Print mixer gains
    buf.write(u"namespace {\n")
    for geom in geom_list:
        # Get desired mix matrix
        if use_normalized_mix:
            mix = geom['mix']['B_px']
        else:
            mix = geom['mix']['B']

        buf.write(u"const MultirotorMixer::Rotor _config_{}[] = {{\n".format(geom['info']['name']))

        for row in mix:
            if use_6dof:
            # 6dof mixer
                buf.write(u"\t{{ {:9f}, {:9f}, {:9f}, {:9f}, {:9f}, {:9f} }},\n".format(
                    row[0], row[1], row[2],
                    row[3], row[4], row[5]))
            else:
            # 4dof mixer
                buf.write(u"\t{{ {:9f}, {:9f}, {:9f}, {:9f} }},\n".format(
                    row[0], row[1], row[2],
                    -row[5]))  # Upward thrust is positive TODO: to remove this, adapt PX4 to use NED correctly

        buf.write(u"};\n\n")

    # Print geom indeces
    buf.write(u"const MultirotorMixer::Rotor *_config_index[] = {\n")
    for geom in geom_list:
        buf.write(u"\t&_config_{}[0],\n".format(geom['info']['name']))
    buf.write(u"};\n\n")

    # Print geom rotor counts
    buf.write(u"const unsigned _config_rotor_count[] = {\n")
    for geom in geom_list:
        buf.write(u"\t{}, /* {} */\n".format(len(geom['rotors']), geom['info']['name']))
    buf.write(u"};\n\n")

    # Print geom key
    buf.write(u"const char* _config_key[] = {\n")
    for geom in geom_list:
        buf.write(u"\t\"{}\",\t/* {} */\n".format(geom['info']['key'], geom['info']['name']))
    buf.write(u"};\n\n")

    # Print footer
    buf.write(u"} // anonymous namespace\n\n")
    buf.write(u"#endif /* _MIXER_MULTI_TABLES */\n\n")

    return buf.getvalue()


if __name__ == '__main__':
    import argparse
    import glob

    # Parse arguments
    parser = argparse.ArgumentParser(
        description='Convert geom .toml files to mixer headers')
    parser.add_argument('-d', dest='dir',
                        help='directory with geom files')
    parser.add_argument('-f', dest='files',
                        help="files to convert (use only without -d)",
                        nargs="+")
    parser.add_argument('-o', dest='outputfile',
                        help='output header file')
    parser.add_argument('--normalize', help='Use normalized mixers (compatibility mode)',
            action='store_true')
    parser.add_argument('--sixdof', help='Use 6dof mixers',
            action='store_true')
    args = parser.parse_args()

    # Find toml files
    if args.files is not None:
        filenames = args.files
    else:
        filenames = glob.glob(args.dir + '/*.toml')

    # List of geometries
    geom_list = []

    for filename in filenames:
        # Parse geom file
        geom = parse_geom_toml(filename)

        # Compute torque and thrust matrices
        A, B = geom_to_mix(geom)

        # Normalize mixer
        B_px = normalize_mix_px4(B)

        # Store matrices in geom
        geom['mix'] = {'A': A, 'B': B, 'B_px': B_px}

        # Add to list
        geom_list.append(geom)

        # print('\nFilename')
        # print(filename)
        # print('\nGeometry')
        # print(geom)
        # print('\nA:')
        # print(A.round(2))
        # print('\nB:')
        # print(B.round(2))
        # print('\nNormalized Mix (as in PX4):')
        # print(B_px)
        # print('\n-----------------------------')

    # Generate header file
    header = generate_mixer_multirotor_header(geom_list,
                                             use_normalized_mix=args.normalize,
                                             use_6dof=args.sixdof)
    # print(header)

    # Write header file
    with open(args.outputfile, 'w') as fd:
        fd.write(header)
