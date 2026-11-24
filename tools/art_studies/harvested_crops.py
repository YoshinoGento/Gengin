"""Plucked specimens for the contest; the two tomato parts share a tabletop origin."""
from math import cos, sin, tau

import numpy as np

from crop_remodel import fruit, tube
from field_crops_revision import pumpkin_stalk
from study_mesh import export_part
from tomato_revision import PROFILE, leaflet


def export_harvest(directory):
    radius, height = .48, .38
    def skin(a, t, w):
        shoulder = cos(5*a+.32)*max(0,t)
        r = radius*w*(1+.025*shoulder)
        return r*cos(a), height*(.98+t+.022*w*shoulder), r*sin(a)
    export_part(directory/'crop_tomato.obj', fruit(32, PROFILE, skin), smooth=True)
    # The shallow socket joins all five sepals; their tips clear the voxel shoulders.
    socket = np.array([0., .636, 0.])
    crown = [tube([socket, (.004,.739,.002), (.024,.792,-.008), (.031,.800,-.011)],
                  [.030,.028,.020,.017], 8)]
    for i in range(5):
        a = .32 + i*tau/5
        axis = np.array([cos(a),0.,sin(a)])
        tangent = np.array([-sin(a),0.,cos(a)])
        crown.append(leaflet([socket, .090*axis+[0,.747,0], .222*axis+[0,.747,0],
                              .335*axis+.025*tangent+[0,.726,0]], [.065,.041],.30))
    export_part(directory/'crop_tomato_stems.obj', np.concatenate(crown))
    # Match the existing pumpkin body and its field-space stalk socket, without its runner.
    stem = pumpkin_stalk(np.array([.040,.015+.65*.688,.006]))
    export_part(directory/'crop_pumpkin_vines.obj', (stem-[0,.020,0])/[.58,.80,.58])
