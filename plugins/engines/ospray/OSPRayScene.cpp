/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
 *
 * This file is part of Brayns <https://github.com/BlueBrain/Brayns>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3.0 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "OSPRayScene.h"
#include "OSPRayRenderer.h"

#include <brayns/common/light/DirectionalLight.h>
#include <brayns/common/light/PointLight.h>
#include <brayns/common/log.h>
#include <brayns/common/material/Texture2D.h>
#include <brayns/common/simulation/AbstractSimulationHandler.h>
#include <brayns/common/volume/BrickedVolumeHandler.h>
#include <brayns/common/volume/VolumeHandler.h>
#include <brayns/io/TextureLoader.h>
#include <brayns/parameters/GeometryParameters.h>
#include <brayns/parameters/SceneParameters.h>

#include <boost/algorithm/string/predicate.hpp> // ends_with

#include <ospcommon/box.h>

namespace brayns
{
const size_t CACHE_VERSION = 7;

std::vector<ospcommon::vec3f> magmaColorMap{
    {ospcommon::vec3f(0.001462, 0.000466, 0.013866)},
    {ospcommon::vec3f(0.002258, 0.001295, 0.018331)},
    {ospcommon::vec3f(0.003279, 0.002305, 0.023708)},
    {ospcommon::vec3f(0.004512, 0.003490, 0.029965)},
    {ospcommon::vec3f(0.005950, 0.004843, 0.037130)},
    {ospcommon::vec3f(0.007588, 0.006356, 0.044973)},
    {ospcommon::vec3f(0.009426, 0.008022, 0.052844)},
    {ospcommon::vec3f(0.011465, 0.009828, 0.060750)},
    {ospcommon::vec3f(0.013708, 0.011771, 0.068667)},
    {ospcommon::vec3f(0.016156, 0.013840, 0.076603)},
    {ospcommon::vec3f(0.018815, 0.016026, 0.084584)},
    {ospcommon::vec3f(0.021692, 0.018320, 0.092610)},
    {ospcommon::vec3f(0.024792, 0.020715, 0.100676)},
    {ospcommon::vec3f(0.028123, 0.023201, 0.108787)},
    {ospcommon::vec3f(0.031696, 0.025765, 0.116965)},
    {ospcommon::vec3f(0.035520, 0.028397, 0.125209)},
    {ospcommon::vec3f(0.039608, 0.031090, 0.133515)},
    {ospcommon::vec3f(0.043830, 0.033830, 0.141886)},
    {ospcommon::vec3f(0.048062, 0.036607, 0.150327)},
    {ospcommon::vec3f(0.052320, 0.039407, 0.158841)},
    {ospcommon::vec3f(0.056615, 0.042160, 0.167446)},
    {ospcommon::vec3f(0.060949, 0.044794, 0.176129)},
    {ospcommon::vec3f(0.065330, 0.047318, 0.184892)},
    {ospcommon::vec3f(0.069764, 0.049726, 0.193735)},
    {ospcommon::vec3f(0.074257, 0.052017, 0.202660)},
    {ospcommon::vec3f(0.078815, 0.054184, 0.211667)},
    {ospcommon::vec3f(0.083446, 0.056225, 0.220755)},
    {ospcommon::vec3f(0.088155, 0.058133, 0.229922)},
    {ospcommon::vec3f(0.092949, 0.059904, 0.239164)},
    {ospcommon::vec3f(0.097833, 0.061531, 0.248477)},
    {ospcommon::vec3f(0.102815, 0.063010, 0.257854)},
    {ospcommon::vec3f(0.107899, 0.064335, 0.267289)},
    {ospcommon::vec3f(0.113094, 0.065492, 0.276784)},
    {ospcommon::vec3f(0.118405, 0.066479, 0.286321)},
    {ospcommon::vec3f(0.123833, 0.067295, 0.295879)},
    {ospcommon::vec3f(0.129380, 0.067935, 0.305443)},
    {ospcommon::vec3f(0.135053, 0.068391, 0.315000)},
    {ospcommon::vec3f(0.140858, 0.068654, 0.324538)},
    {ospcommon::vec3f(0.146785, 0.068738, 0.334011)},
    {ospcommon::vec3f(0.152839, 0.068637, 0.343404)},
    {ospcommon::vec3f(0.159018, 0.068354, 0.352688)},
    {ospcommon::vec3f(0.165308, 0.067911, 0.361816)},
    {ospcommon::vec3f(0.171713, 0.067305, 0.370771)},
    {ospcommon::vec3f(0.178212, 0.066576, 0.379497)},
    {ospcommon::vec3f(0.184801, 0.065732, 0.387973)},
    {ospcommon::vec3f(0.191460, 0.064818, 0.396152)},
    {ospcommon::vec3f(0.198177, 0.063862, 0.404009)},
    {ospcommon::vec3f(0.204935, 0.062907, 0.411514)},
    {ospcommon::vec3f(0.211718, 0.061992, 0.418647)},
    {ospcommon::vec3f(0.218512, 0.061158, 0.425392)},
    {ospcommon::vec3f(0.225302, 0.060445, 0.431742)},
    {ospcommon::vec3f(0.232077, 0.059889, 0.437695)},
    {ospcommon::vec3f(0.238826, 0.059517, 0.443256)},
    {ospcommon::vec3f(0.245543, 0.059352, 0.448436)},
    {ospcommon::vec3f(0.252220, 0.059415, 0.453248)},
    {ospcommon::vec3f(0.258857, 0.059706, 0.457710)},
    {ospcommon::vec3f(0.265447, 0.060237, 0.461840)},
    {ospcommon::vec3f(0.271994, 0.060994, 0.465660)},
    {ospcommon::vec3f(0.278493, 0.061978, 0.469190)},
    {ospcommon::vec3f(0.284951, 0.063168, 0.472451)},
    {ospcommon::vec3f(0.291366, 0.064553, 0.475462)},
    {ospcommon::vec3f(0.297740, 0.066117, 0.478243)},
    {ospcommon::vec3f(0.304081, 0.067835, 0.480812)},
    {ospcommon::vec3f(0.310382, 0.069702, 0.483186)},
    {ospcommon::vec3f(0.316654, 0.071690, 0.485380)},
    {ospcommon::vec3f(0.322899, 0.073782, 0.487408)},
    {ospcommon::vec3f(0.329114, 0.075972, 0.489287)},
    {ospcommon::vec3f(0.335308, 0.078236, 0.491024)},
    {ospcommon::vec3f(0.341482, 0.080564, 0.492631)},
    {ospcommon::vec3f(0.347636, 0.082946, 0.494121)},
    {ospcommon::vec3f(0.353773, 0.085373, 0.495501)},
    {ospcommon::vec3f(0.359898, 0.087831, 0.496778)},
    {ospcommon::vec3f(0.366012, 0.090314, 0.497960)},
    {ospcommon::vec3f(0.372116, 0.092816, 0.499053)},
    {ospcommon::vec3f(0.378211, 0.095332, 0.500067)},
    {ospcommon::vec3f(0.384299, 0.097855, 0.501002)},
    {ospcommon::vec3f(0.390384, 0.100379, 0.501864)},
    {ospcommon::vec3f(0.396467, 0.102902, 0.502658)},
    {ospcommon::vec3f(0.402548, 0.105420, 0.503386)},
    {ospcommon::vec3f(0.408629, 0.107930, 0.504052)},
    {ospcommon::vec3f(0.414709, 0.110431, 0.504662)},
    {ospcommon::vec3f(0.420791, 0.112920, 0.505215)},
    {ospcommon::vec3f(0.426877, 0.115395, 0.505714)},
    {ospcommon::vec3f(0.432967, 0.117855, 0.506160)},
    {ospcommon::vec3f(0.439062, 0.120298, 0.506555)},
    {ospcommon::vec3f(0.445163, 0.122724, 0.506901)},
    {ospcommon::vec3f(0.451271, 0.125132, 0.507198)},
    {ospcommon::vec3f(0.457386, 0.127522, 0.507448)},
    {ospcommon::vec3f(0.463508, 0.129893, 0.507652)},
    {ospcommon::vec3f(0.469640, 0.132245, 0.507809)},
    {ospcommon::vec3f(0.475780, 0.134577, 0.507921)},
    {ospcommon::vec3f(0.481929, 0.136891, 0.507989)},
    {ospcommon::vec3f(0.488088, 0.139186, 0.508011)},
    {ospcommon::vec3f(0.494258, 0.141462, 0.507988)},
    {ospcommon::vec3f(0.500438, 0.143719, 0.507920)},
    {ospcommon::vec3f(0.506629, 0.145958, 0.507806)},
    {ospcommon::vec3f(0.512831, 0.148179, 0.507648)},
    {ospcommon::vec3f(0.519045, 0.150383, 0.507443)},
    {ospcommon::vec3f(0.525270, 0.152569, 0.507192)},
    {ospcommon::vec3f(0.531507, 0.154739, 0.506895)},
    {ospcommon::vec3f(0.537755, 0.156894, 0.506551)},
    {ospcommon::vec3f(0.544015, 0.159033, 0.506159)},
    {ospcommon::vec3f(0.550287, 0.161158, 0.505719)},
    {ospcommon::vec3f(0.556571, 0.163269, 0.505230)},
    {ospcommon::vec3f(0.562866, 0.165368, 0.504692)},
    {ospcommon::vec3f(0.569172, 0.167454, 0.504105)},
    {ospcommon::vec3f(0.575490, 0.169530, 0.503466)},
    {ospcommon::vec3f(0.581819, 0.171596, 0.502777)},
    {ospcommon::vec3f(0.588158, 0.173652, 0.502035)},
    {ospcommon::vec3f(0.594508, 0.175701, 0.501241)},
    {ospcommon::vec3f(0.600868, 0.177743, 0.500394)},
    {ospcommon::vec3f(0.607238, 0.179779, 0.499492)},
    {ospcommon::vec3f(0.613617, 0.181811, 0.498536)},
    {ospcommon::vec3f(0.620005, 0.183840, 0.497524)},
    {ospcommon::vec3f(0.626401, 0.185867, 0.496456)},
    {ospcommon::vec3f(0.632805, 0.187893, 0.495332)},
    {ospcommon::vec3f(0.639216, 0.189921, 0.494150)},
    {ospcommon::vec3f(0.645633, 0.191952, 0.492910)},
    {ospcommon::vec3f(0.652056, 0.193986, 0.491611)},
    {ospcommon::vec3f(0.658483, 0.196027, 0.490253)},
    {ospcommon::vec3f(0.664915, 0.198075, 0.488836)},
    {ospcommon::vec3f(0.671349, 0.200133, 0.487358)},
    {ospcommon::vec3f(0.677786, 0.202203, 0.485819)},
    {ospcommon::vec3f(0.684224, 0.204286, 0.484219)},
    {ospcommon::vec3f(0.690661, 0.206384, 0.482558)},
    {ospcommon::vec3f(0.697098, 0.208501, 0.480835)},
    {ospcommon::vec3f(0.703532, 0.210638, 0.479049)},
    {ospcommon::vec3f(0.709962, 0.212797, 0.477201)},
    {ospcommon::vec3f(0.716387, 0.214982, 0.475290)},
    {ospcommon::vec3f(0.722805, 0.217194, 0.473316)},
    {ospcommon::vec3f(0.729216, 0.219437, 0.471279)},
    {ospcommon::vec3f(0.735616, 0.221713, 0.469180)},
    {ospcommon::vec3f(0.742004, 0.224025, 0.467018)},
    {ospcommon::vec3f(0.748378, 0.226377, 0.464794)},
    {ospcommon::vec3f(0.754737, 0.228772, 0.462509)},
    {ospcommon::vec3f(0.761077, 0.231214, 0.460162)},
    {ospcommon::vec3f(0.767398, 0.233705, 0.457755)},
    {ospcommon::vec3f(0.773695, 0.236249, 0.455289)},
    {ospcommon::vec3f(0.779968, 0.238851, 0.452765)},
    {ospcommon::vec3f(0.786212, 0.241514, 0.450184)},
    {ospcommon::vec3f(0.792427, 0.244242, 0.447543)},
    {ospcommon::vec3f(0.798608, 0.247040, 0.444848)},
    {ospcommon::vec3f(0.804752, 0.249911, 0.442102)},
    {ospcommon::vec3f(0.810855, 0.252861, 0.439305)},
    {ospcommon::vec3f(0.816914, 0.255895, 0.436461)},
    {ospcommon::vec3f(0.822926, 0.259016, 0.433573)},
    {ospcommon::vec3f(0.828886, 0.262229, 0.430644)},
    {ospcommon::vec3f(0.834791, 0.265540, 0.427671)},
    {ospcommon::vec3f(0.840636, 0.268953, 0.424666)},
    {ospcommon::vec3f(0.846416, 0.272473, 0.421631)},
    {ospcommon::vec3f(0.852126, 0.276106, 0.418573)},
    {ospcommon::vec3f(0.857763, 0.279857, 0.415496)},
    {ospcommon::vec3f(0.863320, 0.283729, 0.412403)},
    {ospcommon::vec3f(0.868793, 0.287728, 0.409303)},
    {ospcommon::vec3f(0.874176, 0.291859, 0.406205)},
    {ospcommon::vec3f(0.879464, 0.296125, 0.403118)},
    {ospcommon::vec3f(0.884651, 0.300530, 0.400047)},
    {ospcommon::vec3f(0.889731, 0.305079, 0.397002)},
    {ospcommon::vec3f(0.894700, 0.309773, 0.393995)},
    {ospcommon::vec3f(0.899552, 0.314616, 0.391037)},
    {ospcommon::vec3f(0.904281, 0.319610, 0.388137)},
    {ospcommon::vec3f(0.908884, 0.324755, 0.385308)},
    {ospcommon::vec3f(0.913354, 0.330052, 0.382563)},
    {ospcommon::vec3f(0.917689, 0.335500, 0.379915)},
    {ospcommon::vec3f(0.921884, 0.341098, 0.377376)},
    {ospcommon::vec3f(0.925937, 0.346844, 0.374959)},
    {ospcommon::vec3f(0.929845, 0.352734, 0.372677)},
    {ospcommon::vec3f(0.933606, 0.358764, 0.370541)},
    {ospcommon::vec3f(0.937221, 0.364929, 0.368567)},
    {ospcommon::vec3f(0.940687, 0.371224, 0.366762)},
    {ospcommon::vec3f(0.944006, 0.377643, 0.365136)},
    {ospcommon::vec3f(0.947180, 0.384178, 0.363701)},
    {ospcommon::vec3f(0.950210, 0.390820, 0.362468)},
    {ospcommon::vec3f(0.953099, 0.397563, 0.361438)},
    {ospcommon::vec3f(0.955849, 0.404400, 0.360619)},
    {ospcommon::vec3f(0.958464, 0.411324, 0.360014)},
    {ospcommon::vec3f(0.960949, 0.418323, 0.359630)},
    {ospcommon::vec3f(0.963310, 0.425390, 0.359469)},
    {ospcommon::vec3f(0.965549, 0.432519, 0.359529)},
    {ospcommon::vec3f(0.967671, 0.439703, 0.359810)},
    {ospcommon::vec3f(0.969680, 0.446936, 0.360311)},
    {ospcommon::vec3f(0.971582, 0.454210, 0.361030)},
    {ospcommon::vec3f(0.973381, 0.461520, 0.361965)},
    {ospcommon::vec3f(0.975082, 0.468861, 0.363111)},
    {ospcommon::vec3f(0.976690, 0.476226, 0.364466)},
    {ospcommon::vec3f(0.978210, 0.483612, 0.366025)},
    {ospcommon::vec3f(0.979645, 0.491014, 0.367783)},
    {ospcommon::vec3f(0.981000, 0.498428, 0.369734)},
    {ospcommon::vec3f(0.982279, 0.505851, 0.371874)},
    {ospcommon::vec3f(0.983485, 0.513280, 0.374198)},
    {ospcommon::vec3f(0.984622, 0.520713, 0.376698)},
    {ospcommon::vec3f(0.985693, 0.528148, 0.379371)},
    {ospcommon::vec3f(0.986700, 0.535582, 0.382210)},
    {ospcommon::vec3f(0.987646, 0.543015, 0.385210)},
    {ospcommon::vec3f(0.988533, 0.550446, 0.388365)},
    {ospcommon::vec3f(0.989363, 0.557873, 0.391671)},
    {ospcommon::vec3f(0.990138, 0.565296, 0.395122)},
    {ospcommon::vec3f(0.990871, 0.572706, 0.398714)},
    {ospcommon::vec3f(0.991558, 0.580107, 0.402441)},
    {ospcommon::vec3f(0.992196, 0.587502, 0.406299)},
    {ospcommon::vec3f(0.992785, 0.594891, 0.410283)},
    {ospcommon::vec3f(0.993326, 0.602275, 0.414390)},
    {ospcommon::vec3f(0.993834, 0.609644, 0.418613)},
    {ospcommon::vec3f(0.994309, 0.616999, 0.422950)},
    {ospcommon::vec3f(0.994738, 0.624350, 0.427397)},
    {ospcommon::vec3f(0.995122, 0.631696, 0.431951)},
    {ospcommon::vec3f(0.995480, 0.639027, 0.436607)},
    {ospcommon::vec3f(0.995810, 0.646344, 0.441361)},
    {ospcommon::vec3f(0.996096, 0.653659, 0.446213)},
    {ospcommon::vec3f(0.996341, 0.660969, 0.451160)},
    {ospcommon::vec3f(0.996580, 0.668256, 0.456192)},
    {ospcommon::vec3f(0.996775, 0.675541, 0.461314)},
    {ospcommon::vec3f(0.996925, 0.682828, 0.466526)},
    {ospcommon::vec3f(0.997077, 0.690088, 0.471811)},
    {ospcommon::vec3f(0.997186, 0.697349, 0.477182)},
    {ospcommon::vec3f(0.997254, 0.704611, 0.482635)},
    {ospcommon::vec3f(0.997325, 0.711848, 0.488154)},
    {ospcommon::vec3f(0.997351, 0.719089, 0.493755)},
    {ospcommon::vec3f(0.997351, 0.726324, 0.499428)},
    {ospcommon::vec3f(0.997341, 0.733545, 0.505167)},
    {ospcommon::vec3f(0.997285, 0.740772, 0.510983)},
    {ospcommon::vec3f(0.997228, 0.747981, 0.516859)},
    {ospcommon::vec3f(0.997138, 0.755190, 0.522806)},
    {ospcommon::vec3f(0.997019, 0.762398, 0.528821)},
    {ospcommon::vec3f(0.996898, 0.769591, 0.534892)},
    {ospcommon::vec3f(0.996727, 0.776795, 0.541039)},
    {ospcommon::vec3f(0.996571, 0.783977, 0.547233)},
    {ospcommon::vec3f(0.996369, 0.791167, 0.553499)},
    {ospcommon::vec3f(0.996162, 0.798348, 0.559820)},
    {ospcommon::vec3f(0.995932, 0.805527, 0.566202)},
    {ospcommon::vec3f(0.995680, 0.812706, 0.572645)},
    {ospcommon::vec3f(0.995424, 0.819875, 0.579140)},
    {ospcommon::vec3f(0.995131, 0.827052, 0.585701)},
    {ospcommon::vec3f(0.994851, 0.834213, 0.592307)},
    {ospcommon::vec3f(0.994524, 0.841387, 0.598983)},
    {ospcommon::vec3f(0.994222, 0.848540, 0.605696)},
    {ospcommon::vec3f(0.993866, 0.855711, 0.612482)},
    {ospcommon::vec3f(0.993545, 0.862859, 0.619299)},
    {ospcommon::vec3f(0.993170, 0.870024, 0.626189)},
    {ospcommon::vec3f(0.992831, 0.877168, 0.633109)},
    {ospcommon::vec3f(0.992440, 0.884330, 0.640099)},
    {ospcommon::vec3f(0.992089, 0.891470, 0.647116)},
    {ospcommon::vec3f(0.991688, 0.898627, 0.654202)},
    {ospcommon::vec3f(0.991332, 0.905763, 0.661309)},
    {ospcommon::vec3f(0.990930, 0.912915, 0.668481)},
    {ospcommon::vec3f(0.990570, 0.920049, 0.675675)},
    {ospcommon::vec3f(0.990175, 0.927196, 0.682926)},
    {ospcommon::vec3f(0.989815, 0.934329, 0.690198)},
    {ospcommon::vec3f(0.989434, 0.941470, 0.697519)},
    {ospcommon::vec3f(0.989077, 0.948604, 0.704863)},
    {ospcommon::vec3f(0.988717, 0.955742, 0.712242)},
    {ospcommon::vec3f(0.988367, 0.962878, 0.719649)},
    {ospcommon::vec3f(0.988033, 0.970012, 0.727077)},
    {ospcommon::vec3f(0.987691, 0.977154, 0.734536)},
    {ospcommon::vec3f(0.987387, 0.984288, 0.742002)},
    {ospcommon::vec3f(0.987053, 0.991438, 0.749504)}};

struct TextureTypeMaterialAttribute
{
    TextureType type;
    std::string attribute;
};

static TextureTypeMaterialAttribute textureTypeMaterialAttribute[6] = {
    {TT_DIFFUSE, "map_kd"},  {TT_NORMALS, "map_Normal"},
    {TT_SPECULAR, "map_ks"}, {TT_EMISSIVE, "map_a"},
    {TT_OPACITY, "map_d"},   {TT_REFLECTION, "map_Reflection"}};

OSPRayScene::OSPRayScene(Renderers renderers,
                         ParametersManager& parametersManager)
    : Scene(renderers, parametersManager)
    , _simulationModel(0)
    , _ospLightData(0)
    , _ospMaterialData(0)
    , _ospVolumeData(0)
    , _ospSimulationData(0)
    , _ospTransferFunctionDiffuseData(0)
    , _ospTransferFunctionEmissionData(0)
{
}

void OSPRayScene::reset()
{
    Scene::reset();

    for (const auto& model : _models)
    {
        for (size_t materialId = 0; materialId < _materials.size();
             ++materialId)
        {
            ospRemoveGeometry(model.second, _ospMeshes[materialId]);
            ospRemoveGeometry(model.second, _ospExtendedSpheres[materialId]);
            ospRemoveGeometry(model.second, _ospExtendedCylinders[materialId]);
            ospRemoveGeometry(model.second, _ospExtendedCones[materialId]);
        }
        ospCommit(model.second);
    }

    if (_simulationModel)
    {
        for (size_t materialId = 0; materialId < _materials.size();
             ++materialId)
        {
            ospRemoveGeometry(_simulationModel,
                              _ospExtendedSpheres[materialId]);
            ospRemoveGeometry(_simulationModel,
                              _ospExtendedCylinders[materialId]);
            ospRemoveGeometry(_simulationModel, _ospExtendedCones[materialId]);
        }
        ospCommit(_simulationModel);
    }
    _simulationModel = 0;

    _models.clear();

    _ospMaterials.clear();
    _ospTextures.clear();
    _ospLights.clear();

    _serializedSpheresData.clear();
    _serializedCylindersData.clear();
    _serializedConesData.clear();
    _serializedSpheresDataSize.clear();
    _serializedCylindersDataSize.clear();
    _serializedConesDataSize.clear();

    _timestampSpheresIndices.clear();
    _timestampCylindersIndices.clear();
    _timestampConesIndices.clear();
}

void OSPRayScene::commit()
{
    for (auto model : _models)
        ospCommit(model.second);
    if (_simulationModel)
    {
        BRAYNS_INFO << "Committing simulation model" << std::endl;
        ospCommit(_simulationModel);
    }
}

OSPModel* OSPRayScene::modelImpl(const size_t timestamp)
{
    if (_models.find(timestamp) != _models.end())
        return &_models[timestamp];

    int index = -1;
    for (const auto& model : _models)
        if (model.first <= timestamp)
            index = model.first;
    BRAYNS_DEBUG << "Request model for timestamp " << timestamp << ", returned "
                 << index << std::endl;
    return index == -1 ? nullptr : &_models[index];
}

void OSPRayScene::_saveCacheFile()
{
    const std::string& filename =
        _parametersManager.getGeometryParameters().getSaveCacheFile();
    BRAYNS_INFO << "Saving scene to binary file: " << filename << std::endl;
    std::ofstream file(filename, std::ios::out | std::ios::binary);

    const size_t version = CACHE_VERSION;
    file.write((char*)&version, sizeof(size_t));
    BRAYNS_INFO << "Version: " << version << std::endl;

    const size_t nbModels = _models.size();
    file.write((char*)&nbModels, sizeof(size_t));
    BRAYNS_INFO << nbModels << " models" << std::endl;
    for (const auto& model : _models)
        file.write((char*)&model.first, sizeof(size_t));

    const size_t nbMaterials = _materials.size();
    file.write((char*)&nbMaterials, sizeof(size_t));
    BRAYNS_INFO << nbMaterials << " materials" << std::endl;

    // Save materials
    for (auto& material : _materials)
    {
        size_t id = material.first;
        file.write((char*)&id, sizeof(size_t));
        Vector3f value3f;
        value3f = material.second.getColor();
        file.write((char*)&value3f, sizeof(Vector3f));
        value3f = material.second.getSpecularColor();
        file.write((char*)&value3f, sizeof(Vector3f));
        float value = material.second.getSpecularExponent();
        file.write((char*)&value, sizeof(float));
        value = material.second.getReflectionIndex();
        file.write((char*)&value, sizeof(float));
        value = material.second.getOpacity();
        file.write((char*)&value, sizeof(float));
        value = material.second.getRefractionIndex();
        file.write((char*)&value, sizeof(float));
        value = material.second.getEmission();
        file.write((char*)&value, sizeof(float));
        // TODO: Textures
    }

    // Save geometry
    for (auto& material : _materials)
    {
        const auto materialId = material.first;
        size_t bufferSize;

        // Spheres
        bufferSize = _timestampSpheresIndices[materialId].size();
        file.write((char*)&bufferSize, sizeof(size_t));
        for (const auto& index : _timestampSpheresIndices[materialId])
        {
            file.write((char*)&index.first, sizeof(size_t));
            file.write((char*)&index.second, sizeof(size_t));
        }

        bufferSize = _serializedSpheresDataSize[materialId] *
                     Sphere::getSerializationSize() * sizeof(float);
        file.write((char*)&bufferSize, sizeof(size_t));
        file.write((char*)_serializedSpheresData[materialId].data(),
                   bufferSize);
        if (bufferSize != 0)
            BRAYNS_DEBUG << "[" << materialId << "] "
                         << _serializedSpheresDataSize[materialId] << " Spheres"
                         << std::endl;

        // Cylinders
        bufferSize = _timestampCylindersIndices[materialId].size();
        file.write((char*)&bufferSize, sizeof(size_t));
        for (const auto& index : _timestampCylindersIndices[materialId])
        {
            file.write((char*)&index.first, sizeof(size_t));
            file.write((char*)&index.second, sizeof(size_t));
        }

        bufferSize = _serializedCylindersDataSize[materialId] *
                     Cylinder::getSerializationSize() * sizeof(float);
        file.write((char*)&bufferSize, sizeof(size_t));
        file.write((char*)_serializedCylindersData[materialId].data(),
                   bufferSize);
        if (bufferSize != 0)
            BRAYNS_DEBUG << "[" << materialId << "] "
                         << _serializedCylindersDataSize[materialId]
                         << " Cylinders" << std::endl;

        // Cones
        bufferSize = _timestampConesIndices[materialId].size();
        file.write((char*)&bufferSize, sizeof(size_t));
        for (const auto& index : _timestampConesIndices[materialId])
        {
            file.write((char*)&index.first, sizeof(size_t));
            file.write((char*)&index.second, sizeof(size_t));
        }

        bufferSize = _serializedConesDataSize[materialId] *
                     Cone::getSerializationSize() * sizeof(float);
        file.write((char*)&bufferSize, sizeof(size_t));
        file.write((char*)_serializedConesData[materialId].data(), bufferSize);
        if (bufferSize != 0)
            BRAYNS_DEBUG << "[" << materialId << "] "
                         << _serializedConesDataSize[materialId] << " Cones"
                         << std::endl;

        if (_trianglesMeshes.find(materialId) != _trianglesMeshes.end())
        {
            // Vertices
            bufferSize = _trianglesMeshes[materialId].getVertices().size() *
                         sizeof(Vector3f);
            file.write((char*)&bufferSize, sizeof(size_t));
            file.write((char*)_trianglesMeshes[materialId].getVertices().data(),
                       bufferSize);
            if (bufferSize != 0)
                BRAYNS_DEBUG
                    << "[" << materialId << "] "
                    << _trianglesMeshes[materialId].getVertices().size()
                    << " Vertices" << std::endl;

            // Indices
            bufferSize = _trianglesMeshes[materialId].getIndices().size() *
                         sizeof(Vector3ui);
            file.write((char*)&bufferSize, sizeof(size_t));
            file.write((char*)_trianglesMeshes[materialId].getIndices().data(),
                       bufferSize);
            if (bufferSize != 0)
                BRAYNS_DEBUG << "[" << materialId << "] "
                             << _trianglesMeshes[materialId].getIndices().size()
                             << " Indices" << std::endl;

            // Normals
            bufferSize = _trianglesMeshes[materialId].getNormals().size() *
                         sizeof(Vector3f);
            file.write((char*)&bufferSize, sizeof(size_t));
            file.write((char*)_trianglesMeshes[materialId].getNormals().data(),
                       bufferSize);
            if (bufferSize != 0)
                BRAYNS_DEBUG << "[" << materialId << "] "
                             << _trianglesMeshes[materialId].getNormals().size()
                             << " Normals" << std::endl;

            // Texture coordinates
            bufferSize =
                _trianglesMeshes[materialId].getTextureCoordinates().size() *
                sizeof(Vector2f);
            file.write((char*)&bufferSize, sizeof(size_t));
            file.write((char*)_trianglesMeshes[materialId]
                           .getTextureCoordinates()
                           .data(),
                       bufferSize);
            if (bufferSize != 0)
                BRAYNS_DEBUG << "[" << materialId << "] "
                             << _trianglesMeshes[materialId]
                                    .getTextureCoordinates()
                                    .size()
                             << " Texture coordinates" << std::endl;
        }
        else
        {
            bufferSize = 0;
            file.write((char*)&bufferSize, sizeof(size_t)); // No vertices
            file.write((char*)&bufferSize, sizeof(size_t)); // No indices
            file.write((char*)&bufferSize, sizeof(size_t)); // No normals
            file.write((char*)&bufferSize,
                       sizeof(size_t)); // No Texture coordinates
        }
    }

    file.write((char*)&_bounds, sizeof(Boxf));
    BRAYNS_INFO << _bounds << std::endl;
    file.close();
    BRAYNS_INFO << "Scene successfully saved" << std::endl;
}

void OSPRayScene::_commitBrickedVolumeData()
{
    BrickedVolumeHandlerPtr brickedVolumeHandler = getBrickedVolumeHandler();
    if (!brickedVolumeHandler)
        return;

    const bool useAMR = false;
    if (_ospVolume)
    {
        const auto& vol = _scene.getVolume();
        ospSet1i(_ospVolume, "singleShade", vol.getSingleShade());
        ospSet1i(_ospVolume, "gradientShadingEnabled",
                 vol.getGradientShading());
        ospSet1f(_ospVolume, "adaptiveScalar", vol.getAdaptiveScalar());
        ospSet1f(_ospVolume, "adaptiveMaxSamplingRate",
                 vol.getAdaptiveMaxSamplingRate());
        ospSet1f(_ospVolume, "adaptiveBacktrack", vol.getAdaptiveBacktrack());
        ospSet1i(_ospVolume, "adaptiveSampling", vol.getAdaptiveSampling());
        ospSet1f(_ospVolume, "samplingRate", vol.getSamplingRate());
        osp::vec3f specular = osp::vec3f{0.135f, 0.135f, 0.135f};
        ospSet3fv(_ospVolume, "specular", &specular.x);
        ospCommit(_ospVolume);
        return;
    }

    commitLights();

    ospcommon::vec2f valueRange{0, 255};

    Boxf& worldBounds = getWorldBounds();
    worldBounds.reset();

    const int lod = 1;
    auto visibles = brickedVolumeHandler->getVisibles(lod);
    std::cout << visibles.size() << std::endl;

    if (useAMR)
    {
        ospLoadModule("amr");

        struct BrickInfo
        {
            ospcommon::box3i box;
            int level = 0;
            float dt = 1.0;

            ospcommon::vec3i size() const
            {
                return box.size() + ospcommon::vec3i(1);
            }
        };

        std::vector<BrickInfo> brickInfo;
        std::vector<OSPData> brickData;

        brickInfo.reserve(visibles.size());
        brickData.reserve(visibles.size());
        for (const auto& nodeID : visibles)
        {
            BrickInfo brick;
            const auto position = brickedVolumeHandler->getPosition(nodeID);
            const auto voxelBox = brickedVolumeHandler->getVoxelBox(nodeID);
            auto lower = ospcommon::vec3i{int(position.x()), int(position.y()),
                                          int(position.z())};
            auto upper = lower + ospcommon::vec3i{int(voxelBox.x()) - 1,
                                                  int(voxelBox.y()) - 1,
                                                  int(voxelBox.z()) - 1};

            brick.box = {lower, upper};
            brickInfo.push_back(brick);

            worldBounds.merge(position);
            worldBounds.merge(voxelBox);

            OSPData data =
                ospNewData(brickedVolumeHandler->getVoxelBox(nodeID).product(),
                           OSP_FLOAT,
                           brickedVolumeHandler->getData(nodeID).get(), 0);
            brickData.push_back(data);
        }

        _ospVolume = ospNewVolume("chombo_volume");

        _brickDataData =
            ospNewData(brickData.size(), OSP_OBJECT, &brickData[0], 0);
        ospSetData(_ospVolume, "brickData", _brickDataData);
        _brickInfoData = ospNewData(brickInfo.size() * sizeof(brickInfo[0]),
                                    OSP_RAW, &brickInfo[0], 0);
        ospSetData(_ospVolume, "brickInfo", _brickInfoData);
    }
    else
    {
        _ospVolume = ospNewVolume("block_bricked_volume");

        const auto dim = brickedVolumeHandler->getDimension(lod);
        const ospcommon::vec3i dimension = {dim.x(), dim.y(), dim.z()};
        ospSetVec3i(_ospVolume, "dimensions", (osp::vec3i&)dimension);

        const auto gs = brickedVolumeHandler->getGridSpacing(lod);
        ospcommon::vec3f gridSpacing = {gs.x(), gs.y(), gs.z()};
        ospSetVec3f(_ospVolume, "gridSpacing", (osp::vec3f&)gridSpacing);

        worldBounds.merge(-(dim * gs) / 2);
        worldBounds.merge(dim * gs);

        switch (brickedVolumeHandler->getDataType())
        {
        case livre::DT_FLOAT:
            ospSetString(_ospVolume, "voxelType", "float");
            break;
        case livre::DT_UINT16:
            ospSetString(_ospVolume, "voxelType", "ushort");
            valueRange = {12500, 40000}; // beechnut
            break;
        case livre::DT_UINT32:
            ospSetString(_ospVolume, "voxelType", "uint");
            break;
        case livre::DT_INT8:
            ospSetString(_ospVolume, "voxelType", "char");
            break;
        case livre::DT_INT16:
            ospSetString(_ospVolume, "voxelType", "short");
            break;
        case livre::DT_INT32:
            ospSetString(_ospVolume, "voxelType", "int");
            break;
        case livre::DT_UINT8:
        default:
            ospSetString(_ospVolume, "voxelType", "uchar");
            break;
        }

        visibles.push_back({1, {0, 0, 0}});
        for (const auto& nodeID : visibles)
        {
            const auto position = brickedVolumeHandler->getPosition(nodeID);
            const auto voxelBox = brickedVolumeHandler->getVoxelBox(nodeID);

            ospSetRegion(_ospVolume, brickedVolumeHandler->getRawData(nodeID),
                         osp::vec3i{int(position.x()), int(position.y()),
                                    int(position.z())},
                         osp::vec3i{int(voxelBox.x()), int(voxelBox.y()),
                                    int(voxelBox.z())});
        }
    }

    ospSet1i(_ospVolume, "singleShade", 1);
    ospSet1i(_ospVolume, "preIntegration", 1);
    ospSet1i(_ospVolume, "gradientShadingEnabled", 0);
    ospSet1i(_ospVolume, "adaptiveSampling", 0);
    ospSet2f(_ospVolume, "voxelRange", valueRange.x, valueRange.y);

    _ospTransferFunction = ospNewTransferFunction("piecewise_linear");
    std::vector<float> opacityValues(256, 0.01f);
    for (size_t i = 0; i < 100; ++i)
        opacityValues[i] = 0.f;
    OSPData opacityValuesData =
        ospNewData(opacityValues.size(), OSP_FLOAT, opacityValues.data());
    ospSetData(_ospTransferFunction, "opacities", opacityValuesData);
    ospSet2f(_ospTransferFunction, "valueRange", valueRange.x, valueRange.y);

    OSPData colorsData =
        ospNewData(magmaColorMap.size(), OSP_FLOAT3, magmaColorMap.data());
    ospSetData(_ospTransferFunction, "colors", colorsData);
    ospCommit(_ospTransferFunction);
    ospSetObject(_ospVolume, "transferFunction", _ospTransferFunction);

    ospCommit(_ospVolume);

    if (_models.empty())
        _models[0] = ospNewModel();

    ospAddVolume(_models[0], _ospVolume);
}

void OSPRayScene::_loadCacheFile()
{
    const std::string& filename =
        _parametersManager.getGeometryParameters().getLoadCacheFile();
    BRAYNS_INFO << "Loading scene from binary file: " << filename << std::endl;
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file.good())
    {
        BRAYNS_ERROR << "Could not open cache file " << filename << std::endl;
        return;
    }

    size_t version;
    file.read((char*)&version, sizeof(size_t));
    BRAYNS_INFO << "Version: " << version << std::endl;

    if (version != CACHE_VERSION)
    {
        BRAYNS_ERROR << "Only version " << CACHE_VERSION << " is supported"
                     << std::endl;
        return;
    }

    _models.clear();
    size_t nbModels;
    file.read((char*)&nbModels, sizeof(size_t));
    BRAYNS_INFO << nbModels << " models" << std::endl;
    for (size_t model = 0; model < nbModels; ++model)
    {
        size_t ts;
        file.read((char*)&ts, sizeof(size_t));
        BRAYNS_INFO << "Model for ts " << ts << " created" << std::endl;
        _models[ts] = ospNewModel();
    }

    size_t nbMaterials;
    file.read((char*)&nbMaterials, sizeof(size_t));
    BRAYNS_INFO << nbMaterials << " materials" << std::endl;

    // Read materials
    _materials.clear();
    buildMaterials();
    for (size_t i = 0; i < nbMaterials; ++i)
    {
        size_t id;
        file.read((char*)&id, sizeof(size_t));
        auto& material = _materials[id];
        Vector3f value3f;
        file.read((char*)&value3f, sizeof(Vector3f));
        material.setColor(value3f);
        file.read((char*)&value3f, sizeof(Vector3f));
        material.setSpecularColor(value3f);
        float value;
        file.read((char*)&value, sizeof(float));
        material.setSpecularExponent(value);
        file.read((char*)&value, sizeof(float));
        material.setReflectionIndex(value);
        file.read((char*)&value, sizeof(float));
        material.setOpacity(value);
        file.read((char*)&value, sizeof(float));
        material.setRefractionIndex(value);
        file.read((char*)&value, sizeof(float));
        material.setEmission(value);
        // TODO: Textures
    }
    commitMaterials();

    // Read geometry
    for (size_t materialId = 0; materialId < nbMaterials; ++materialId)
    {
        // Spheres
        size_t bufferSize = 0;

        file.read((char*)&bufferSize, sizeof(size_t));
        for (size_t i = 0; i < bufferSize; ++i)
        {
            size_t ts;
            file.read((char*)&ts, sizeof(size_t));
            size_t index;
            file.read((char*)&index, sizeof(size_t));
            _timestampSpheresIndices[materialId][ts] = index;
        }

        file.read((char*)&bufferSize, sizeof(size_t));
        _serializedSpheresDataSize[materialId] =
            bufferSize / (Sphere::getSerializationSize() * sizeof(float));
        if (bufferSize != 0)
        {
            BRAYNS_DEBUG << "[" << materialId << "] "
                         << _serializedSpheresDataSize[materialId] << " Spheres"
                         << std::endl;
            _serializedSpheresData[materialId].resize(bufferSize);
            file.read((char*)_serializedSpheresData[materialId].data(),
                      bufferSize);
        }
        _serializeSpheres(materialId);

        // Cylinders
        bufferSize = 0;
        file.read((char*)&bufferSize, sizeof(size_t));
        for (size_t i = 0; i < bufferSize; ++i)
        {
            size_t ts;
            file.read((char*)&ts, sizeof(size_t));
            size_t index;
            file.read((char*)&index, sizeof(size_t));
            _timestampCylindersIndices[materialId][ts] = index;
        }

        file.read((char*)&bufferSize, sizeof(size_t));
        _serializedCylindersDataSize[materialId] =
            bufferSize / (Cylinder::getSerializationSize() * sizeof(float));
        if (bufferSize != 0)
        {
            BRAYNS_DEBUG << "[" << materialId << "] "
                         << _serializedCylindersDataSize[materialId]
                         << " Cylinders" << std::endl;
            _serializedCylindersData[materialId].reserve(bufferSize);
            file.read((char*)_serializedCylindersData[materialId].data(),
                      bufferSize);
        }
        _serializeCylinders(materialId);

        // Cones
        bufferSize = 0;
        file.read((char*)&bufferSize, sizeof(size_t));
        for (size_t i = 0; i < bufferSize; ++i)
        {
            size_t ts;
            file.read((char*)&ts, sizeof(size_t));
            size_t index;
            file.read((char*)&index, sizeof(size_t));
            _timestampConesIndices[materialId][ts] = index;
        }

        file.read((char*)&bufferSize, sizeof(size_t));
        _serializedConesDataSize[materialId] =
            bufferSize / (Cone::getSerializationSize() * sizeof(float));
        if (bufferSize != 0)
        {
            BRAYNS_DEBUG << "[" << materialId << "] "
                         << _serializedConesDataSize[materialId] << " Cones"
                         << std::endl;
            _serializedConesData[materialId].reserve(bufferSize);
            file.read((char*)_serializedConesData[materialId].data(),
                      bufferSize);
        }
        _serializeCones(materialId);

        // Vertices
        _trianglesMeshes[materialId].getVertices().clear();
        bufferSize = 0;
        file.read((char*)&bufferSize, sizeof(size_t));
        for (size_t i = 0; i < bufferSize / sizeof(Vector3f); ++i)
        {
            Vector3f vertex;
            file.read((char*)&vertex, sizeof(Vector3f));
            _trianglesMeshes[materialId].getVertices().push_back(vertex);
        }

        // Indices
        _trianglesMeshes[materialId].getIndices().clear();
        bufferSize = 0;
        file.read((char*)&bufferSize, sizeof(size_t));
        for (size_t i = 0; i < bufferSize / sizeof(Vector3ui); ++i)
        {
            Vector3ui index;
            file.read((char*)&index, sizeof(Vector3ui));
            _trianglesMeshes[materialId].getIndices().push_back(index);
        }

        // Normals
        _trianglesMeshes[materialId].getNormals().clear();
        bufferSize = 0;
        file.read((char*)&bufferSize, sizeof(size_t));
        for (size_t i = 0; i < bufferSize / sizeof(Vector3f); ++i)
        {
            Vector3f normal;
            file.read((char*)&normal, sizeof(Vector3f));
            _trianglesMeshes[materialId].getNormals().push_back(normal);
        }

        // Texture coordinates
        _trianglesMeshes[materialId].getTextureCoordinates().clear();
        bufferSize = 0;
        file.read((char*)&bufferSize, sizeof(size_t));
        for (size_t i = 0; i < bufferSize / sizeof(Vector2f); ++i)
        {
            Vector2f texCoord;
            file.read((char*)&texCoord, sizeof(Vector2f));
            _trianglesMeshes[materialId].getTextureCoordinates().push_back(
                texCoord);
        }

        _buildMeshOSPGeometry(materialId);
    }

    // Scene bounds
    file.read((char*)&_bounds, sizeof(Boxf));

    BRAYNS_INFO << _bounds << std::endl;
    BRAYNS_INFO << "Scene successfully loaded" << std::endl;
    file.close();
}

void OSPRayScene::_createModel(const size_t timestamp)
{
    if (_models.find(timestamp) == _models.end())
    {
        _models[timestamp] = ospNewModel();
        BRAYNS_INFO << "Model created for timestamp " << timestamp << ": "
                    << _models[timestamp] << std::endl;
    }
}

uint64_t OSPRayScene::_serializeSpheres(const size_t materialId)
{
    const auto& geometryParameters = _parametersManager.getGeometryParameters();
    uint64_t size = 0;

    size_t count = 0;
    if (_spheres.find(materialId) != _spheres.end())
        for (const auto& sphere : _spheres[materialId])
        {
            const float ts =
                (_models.size() == 1) ? 0.f : sphere->getTimestamp();
            size += sphere->serializeData(_serializedSpheresData[materialId]);
            ++_serializedSpheresDataSize[materialId];
            _timestampSpheresIndices[materialId][ts] = ++count;
        }

    // Extended spheres
    for (const auto& timestampSpheresIndex :
         _timestampSpheresIndices[materialId])
    {
        const size_t spheresBufferSize =
            timestampSpheresIndex.second * Sphere::getSerializationSize();

        for (const auto& model : _models)
        {
            if (timestampSpheresIndex.first <= model.first)
            {
                if (_ospExtendedSpheres.find(materialId) !=
                    _ospExtendedSpheres.end())
                    ospRemoveGeometry(model.second,
                                      _ospExtendedSpheres[materialId]);

                _ospExtendedSpheres[materialId] =
                    ospNewGeometry("extendedspheres");

                _ospExtendedSpheresData[materialId] =
                    ospNewData(spheresBufferSize, OSP_FLOAT,
                               &_serializedSpheresData[materialId][0],
                               _getOSPDataFlags());

                ospSetObject(_ospExtendedSpheres[materialId], "extendedspheres",
                             _ospExtendedSpheresData[materialId]);
                ospSet1i(_ospExtendedSpheres[materialId],
                         "bytes_per_extended_sphere",
                         Sphere::getSerializationSize() * sizeof(float));
                ospSet1i(_ospExtendedSpheres[materialId], "materialID",
                         materialId);
                ospSet1i(_ospExtendedSpheres[materialId], "offset_radius",
                         3 * sizeof(float));
                ospSet1i(_ospExtendedSpheres[materialId], "offset_timestamp",
                         4 * sizeof(float));
                ospSet1i(_ospExtendedSpheres[materialId], "offset_value",
                         5 * sizeof(float));

                if (_ospMaterials[materialId])
                    ospSetMaterial(_ospExtendedSpheres[materialId],
                                   _ospMaterials[materialId]);

                ospCommit(_ospExtendedSpheres[materialId]);

                if (geometryParameters.getUseSimulationModel())
                    ospAddGeometry(_simulationModel,
                                   _ospExtendedSpheres[materialId]);
                else
                    ospAddGeometry(model.second,
                                   _ospExtendedSpheres[materialId]);
            }
        }
    }
    return size;
}

uint64_t OSPRayScene::_serializeCylinders(const size_t materialId)
{
    const auto& geometryParameters = _parametersManager.getGeometryParameters();
    uint64_t size = 0;

    size_t count = 0;
    if (_cylinders.find(materialId) != _cylinders.end())
        for (const auto& cylinder : _cylinders[materialId])
        {
            const float ts =
                (_models.size() == 1) ? 0.f : cylinder->getTimestamp();
            size +=
                cylinder->serializeData(_serializedCylindersData[materialId]);
            ++_serializedCylindersDataSize[materialId];
            _timestampCylindersIndices[materialId][ts] = ++count;
        }

    // Extended cylinders
    for (const auto& timestampCylindersIndex :
         _timestampCylindersIndices[materialId])
    {
        const size_t cylindersBufferSize =
            timestampCylindersIndex.second * Cylinder::getSerializationSize();

        for (const auto& model : _models)
        {
            if (timestampCylindersIndex.first <= model.first)
            {
                if (_ospExtendedCylinders.find(materialId) !=
                    _ospExtendedCylinders.end())
                    ospRemoveGeometry(model.second,
                                      _ospExtendedCylinders[materialId]);

                _ospExtendedCylinders[materialId] =
                    ospNewGeometry("extendedcylinders");
                assert(_ospExtendedCylinders[materialId]);

                _ospExtendedCylindersData[materialId] =
                    ospNewData(cylindersBufferSize, OSP_FLOAT,
                               &_serializedCylindersData[materialId][0],
                               _getOSPDataFlags());

                ospSet1i(_ospExtendedCylinders[materialId], "materialID",
                         materialId);
                ospSetObject(_ospExtendedCylinders[materialId],
                             "extendedcylinders",
                             _ospExtendedCylindersData[materialId]);
                ospSet1i(_ospExtendedCylinders[materialId],
                         "bytes_per_extended_cylinder",
                         Cylinder::getSerializationSize() * sizeof(float));
                ospSet1i(_ospExtendedCylinders[materialId], "offset_timestamp",
                         7 * sizeof(float));
                ospSet1i(_ospExtendedCylinders[materialId], "offset_value",
                         8 * sizeof(float));

                if (_ospMaterials[materialId])
                    ospSetMaterial(_ospExtendedCylinders[materialId],
                                   _ospMaterials[materialId]);

                ospCommit(_ospExtendedCylinders[materialId]);
                if (geometryParameters.getUseSimulationModel())
                    ospAddGeometry(_simulationModel,
                                   _ospExtendedCylinders[materialId]);
                else
                    ospAddGeometry(model.second,
                                   _ospExtendedCylinders[materialId]);
            }
        }
    }
    return size;
}

uint64_t OSPRayScene::_serializeCones(const size_t materialId)
{
    const auto& geometryParameters = _parametersManager.getGeometryParameters();
    uint64_t size = 0;

    size_t count = 0;
    if (_cones.find(materialId) != _cones.end())
        for (const auto& cone : _cones[materialId])
        {
            const float ts = (_models.size() == 1) ? 0.f : cone->getTimestamp();
            size += cone->serializeData(_serializedConesData[materialId]);
            ++_serializedConesDataSize[materialId];
            _timestampConesIndices[materialId][ts] = ++count;
        }

    // Extended cones
    for (const auto& timestampConesIndex : _timestampConesIndices[materialId])
    {
        const size_t conesBufferSize =
            timestampConesIndex.second * Cone::getSerializationSize();

        for (const auto& model : _models)
        {
            if (timestampConesIndex.first <= model.first)
            {
                if (_ospExtendedCones.find(materialId) !=
                    _ospExtendedCones.end())
                    ospRemoveGeometry(model.second,
                                      _ospExtendedCones[materialId]);

                _ospExtendedCones[materialId] = ospNewGeometry("extendedcones");
                assert(_ospExtendedCones[materialId]);

                _ospExtendedConesData[materialId] =
                    ospNewData(conesBufferSize, OSP_FLOAT,
                               &_serializedConesData[materialId][0],
                               _getOSPDataFlags());

                ospSet1i(_ospExtendedCones[materialId], "materialID",
                         materialId);
                ospSetObject(_ospExtendedCones[materialId], "extendedcones",
                             _ospExtendedConesData[materialId]);
                ospSet1i(_ospExtendedCones[materialId],
                         "bytes_per_extended_cone",
                         Cone::getSerializationSize() * sizeof(float));
                ospSet1i(_ospExtendedCones[materialId], "offset_timestamp",
                         8 * sizeof(float));
                ospSet1i(_ospExtendedCones[materialId], "offset_value",
                         9 * sizeof(float));

                if (_ospMaterials[materialId])
                    ospSetMaterial(_ospExtendedCones[materialId],
                                   _ospMaterials[materialId]);

                ospCommit(_ospExtendedCones[materialId]);
                if (geometryParameters.getUseSimulationModel())
                    ospAddGeometry(_simulationModel,
                                   _ospExtendedCones[materialId]);
                else
                    ospAddGeometry(model.second, _ospExtendedCones[materialId]);
            }
        }
    }
    return size;
}

uint64_t OSPRayScene::serializeGeometry()
{
    uint64_t size = 0;

    if (_spheresDirty)
    {
        _serializedSpheresDataSize.clear();
        _timestampSpheresIndices.clear();
        _serializedSpheresData.clear();
        for (auto& material : _materials)
        {
            const auto materialId = material.first;
            _serializedSpheresDataSize[materialId] = 0;
            size += _serializeSpheres(materialId);
        }
        _spheresDirty = false;
    }

    if (_cylindersDirty)
    {
        _serializedCylindersData.clear();
        _serializedCylindersDataSize.clear();
        _timestampCylindersIndices.clear();
        for (auto& material : _materials)
        {
            const auto materialId = material.first;
            _serializedCylindersDataSize[materialId] = 0;
            size += _serializeCylinders(materialId);
        }
        _cylindersDirty = false;
    }

    if (_conesDirty)
    {
        _serializedConesData.clear();
        _serializedConesDataSize.clear();
        _timestampConesIndices.clear();
        for (auto& material : _materials)
        {
            const auto materialId = material.first;
            _serializedConesDataSize[materialId] = 0;
            size += _serializeCones(materialId);
        }
        _conesDirty = false;
    }

    // Triangle meshes
    if (_trianglesMeshesDirty)
    {
        for (auto& material : _materials)
            size += _buildMeshOSPGeometry(material.first);
        _trianglesMeshesDirty = false;
    }
    return size;
}

void OSPRayScene::buildGeometry()
{
    BRAYNS_INFO << "Building OSPRay geometry" << std::endl;

    commitMaterials();

    if (_parametersManager.getGeometryParameters().getGenerateMultipleModels())
        // Initialize models according to timestamps
        for (auto& material : _materials)
        {
            for (const auto& sphere : _spheres[material.first])
                _createModel(sphere->getTimestamp());
            for (const auto& cylinder : _cylinders[material.first])
                _createModel(cylinder->getTimestamp());
            for (const auto& cone : _cones[material.first])
                _createModel(cone->getTimestamp());
        }

    if (_models.size() == 0)
        // If no timestamp is available, create a default model at timestamp 0
        _models[0] = ospNewModel();

    if (_parametersManager.getGeometryParameters().getUseSimulationModel())
        _simulationModel = ospNewModel();

    BRAYNS_INFO << "Models to process: " << _models.size() << std::endl;

    uint64_t size = serializeGeometry();
    commitLights();

    if (!_parametersManager.getGeometryParameters().getLoadCacheFile().empty())
        _loadCacheFile();

    size_t totalNbSpheres = 0;
    size_t totalNbCylinders = 0;
    size_t totalNbCones = 0;
    size_t totalNbVertices = 0;
    size_t totalNbIndices = 0;
    for (auto& material : _materials)
    {
        const auto materialId = material.first;
        totalNbSpheres += _serializedSpheresDataSize[materialId];
        totalNbCylinders += _serializedCylindersDataSize[materialId];
        totalNbCones += _serializedConesDataSize[materialId];
        if (_trianglesMeshes.find(materialId) != _trianglesMeshes.end())
        {
            totalNbVertices +=
                _trianglesMeshes[materialId].getVertices().size();
            totalNbIndices += _trianglesMeshes[materialId].getIndices().size();
        }
    }

    BRAYNS_INFO << "---------------------------------------------------"
                << std::endl;
    BRAYNS_INFO << "Geometry information" << std::endl;
    BRAYNS_INFO << "Spheres  : " << totalNbSpheres << std::endl;
    BRAYNS_INFO << "Cylinders: " << totalNbCylinders << std::endl;
    BRAYNS_INFO << "Cones    : " << totalNbCones << std::endl;
    BRAYNS_INFO << "Vertices : " << totalNbVertices << std::endl;
    BRAYNS_INFO << "Indices  : " << totalNbIndices << std::endl;
    BRAYNS_INFO << "Materials: " << _materials.size() << std::endl;
    BRAYNS_INFO << "Total    : " << size << " bytes" << std::endl;
    BRAYNS_INFO << "---------------------------------------------------"
                << std::endl;

    if (!_parametersManager.getGeometryParameters().getSaveCacheFile().empty())
        _saveCacheFile();
}

uint64_t OSPRayScene::_buildMeshOSPGeometry(const size_t materialId)
{
    uint64_t size = 0;
    // Triangle meshes
    if (_trianglesMeshes.find(materialId) != _trianglesMeshes.end())
    {
        _ospMeshes[materialId] = ospNewGeometry("trianglemesh");
        assert(_ospMeshes[materialId]);

        size += _trianglesMeshes[materialId].getVertices().size() * 3 *
                sizeof(float);
        OSPData vertices =
            ospNewData(_trianglesMeshes[materialId].getVertices().size(),
                       OSP_FLOAT3,
                       &_trianglesMeshes[materialId].getVertices()[0],
                       _getOSPDataFlags());

        size += _trianglesMeshes[materialId].getNormals().size() * 3 *
                sizeof(float);
        OSPData normals =
            ospNewData(_trianglesMeshes[materialId].getNormals().size(),
                       OSP_FLOAT3,
                       &_trianglesMeshes[materialId].getNormals()[0],
                       _getOSPDataFlags());

        size +=
            _trianglesMeshes[materialId].getIndices().size() * 3 * sizeof(int);
        OSPData indices =
            ospNewData(_trianglesMeshes[materialId].getIndices().size(),
                       OSP_INT3, &_trianglesMeshes[materialId].getIndices()[0],
                       _getOSPDataFlags());

        size +=
            _trianglesMeshes[materialId].getColors().size() * 4 * sizeof(float);
        OSPData colors =
            ospNewData(_trianglesMeshes[materialId].getColors().size(),
                       OSP_FLOAT3A,
                       &_trianglesMeshes[materialId].getColors()[0],
                       _getOSPDataFlags());

        size += _trianglesMeshes[materialId].getTextureCoordinates().size() *
                2 * sizeof(float);
        OSPData texCoords = ospNewData(
            _trianglesMeshes[materialId].getTextureCoordinates().size(),
            OSP_FLOAT2,
            &_trianglesMeshes[materialId].getTextureCoordinates()[0],
            _getOSPDataFlags());

        ospSetObject(_ospMeshes[materialId], "position", vertices);
        ospSetObject(_ospMeshes[materialId], "index", indices);
        ospSetObject(_ospMeshes[materialId], "vertex.normal", normals);
        ospSetObject(_ospMeshes[materialId], "vertex.color", colors);
        ospSetObject(_ospMeshes[materialId], "vertex.texcoord", texCoords);
        ospSet1i(_ospMeshes[materialId], "alpha_type", 0);
        ospSet1i(_ospMeshes[materialId], "alpha_component", 4);

        if (_ospMaterials[materialId])
            ospSetMaterial(_ospMeshes[materialId], _ospMaterials[materialId]);

        ospCommit(_ospMeshes[materialId]);

        // Meshes are by default added to all timestamps
        for (const auto& model : _models)
            ospAddGeometry(model.second, _ospMeshes[materialId]);
    }
    return size;
}

void OSPRayScene::commitLights()
{
    for (auto renderer : _renderers)
    {
        OSPRayRenderer* osprayRenderer =
            dynamic_cast<OSPRayRenderer*>(renderer.get());

        size_t lightCount = 0;
        for (auto light : _lights)
        {
            DirectionalLight* directionalLight =
                dynamic_cast<DirectionalLight*>(light.get());
            if (directionalLight != 0)
            {
                if (_ospLights.size() <= lightCount)
                    _ospLights.push_back(ospNewLight(osprayRenderer->impl(),
                                                     "DirectionalLight"));

                const Vector3f color = directionalLight->getColor();
                ospSet3f(_ospLights[lightCount], "color", color.x(), color.y(),
                         color.z());
                const Vector3f direction = directionalLight->getDirection();
                ospSet3f(_ospLights[lightCount], "direction", direction.x(),
                         direction.y(), direction.z());
                ospSet1f(_ospLights[lightCount], "intensity",
                         directionalLight->getIntensity());
                ospCommit(_ospLights[lightCount]);
            }
            else
            {
                PointLight* pointLight = dynamic_cast<PointLight*>(light.get());
                if (pointLight != 0)
                {
                    if (_ospLights.size() <= lightCount)
                        _ospLights.push_back(
                            ospNewLight(osprayRenderer->impl(), "PointLight"));

                    const Vector3f position = pointLight->getPosition();
                    ospSet3f(_ospLights[lightCount], "position", position.x(),
                             position.y(), position.z());
                    const Vector3f color = pointLight->getColor();
                    ospSet3f(_ospLights[lightCount], "color", color.x(),
                             color.y(), color.z());
                    ospSet1f(_ospLights[lightCount], "intensity",
                             pointLight->getIntensity());
                    ospSet1f(_ospLights[lightCount], "radius",
                             pointLight->getCutoffDistance());
                    ospCommit(_ospLights[lightCount]);
                }
            }
            ++lightCount;
        }

        if (_ospLightData == 0)
        {
            auto ambientLight =
                ospNewLight(osprayRenderer->impl(), "AmbientLight");
            ospSet3f(ambientLight, "color", 1.f, 1.f, 1.f);
            ospCommit(ambientLight);
            _ospLights.push_back(ambientLight);

            _ospLightData = ospNewData(_ospLights.size(), OSP_OBJECT,
                                       &_ospLights[0], _getOSPDataFlags());
            ospCommit(_ospLightData);
        }
        ospSetData(osprayRenderer->impl(), "lights", _ospLightData);
    }
}

void OSPRayScene::commitMaterials(const bool updateOnly)
{
    _ospMaterialData = 0;

    // Determine how many materials need to be created
    size_t maxId = 0;
    for (auto& material : _materials)
        maxId = std::max(maxId, material.first);

    for (size_t i = 0; i < maxId; ++i)
        if (_materials.find(i) == _materials.end())
            _materials[i] = Material();

    BRAYNS_INFO << "Committing " << maxId + 1 << " OSPRay materials"
                << std::endl;

    for (auto& material : _materials)
    {
        if (_ospMaterials.size() <= material.first)
            for (const auto& renderer : _renderers)
            {
                OSPRayRenderer* osprayRenderer =
                    dynamic_cast<OSPRayRenderer*>(renderer.get());
                _ospMaterials.push_back(ospNewMaterial(osprayRenderer->impl(),
                                                       "ExtendedOBJMaterial"));
            }

        auto& ospMaterial = _ospMaterials[material.first];

        Vector3f value3f = material.second.getColor();
        ospSet3f(ospMaterial, "kd", value3f.x(), value3f.y(), value3f.z());
        value3f = material.second.getSpecularColor();
        ospSet3f(ospMaterial, "ks", value3f.x(), value3f.y(), value3f.z());
        ospSet1f(ospMaterial, "ns", material.second.getSpecularExponent());
        ospSet1f(ospMaterial, "d", material.second.getOpacity());
        ospSet1f(ospMaterial, "refraction",
                 material.second.getRefractionIndex());
        ospSet1f(ospMaterial, "reflection",
                 material.second.getReflectionIndex());
        ospSet1f(ospMaterial, "a", material.second.getEmission());
        ospSet1f(ospMaterial, "g", material.second.getGlossiness());

        if (!updateOnly)
        {
            // Textures
            for (auto texture : material.second.getTextures())
            {
                TextureLoader textureLoader;
                if (texture.second != TEXTURE_NAME_SIMULATION)
                    textureLoader.loadTexture(_textures, texture.first,
                                              texture.second);
                else
                    BRAYNS_ERROR << "Failed to load texture: " << texture.second
                                 << std::endl;

                OSPTexture2D ospTexture = _createTexture2D(texture.second);
                ospSetObject(ospMaterial,
                             textureTypeMaterialAttribute[texture.first]
                                 .attribute.c_str(),
                             ospTexture);

                BRAYNS_DEBUG
                    << "Texture assigned to "
                    << textureTypeMaterialAttribute[texture.first].attribute
                    << " of material " << material.first << ": "
                    << texture.second << std::endl;
            }
        }
        ospCommit(ospMaterial);
    }

    _ospMaterialData = ospNewData(_materials.size(), OSP_OBJECT,
                                  &_ospMaterials[0], _getOSPDataFlags());
    ospCommit(_ospMaterialData);

    for (const auto& renderer : _renderers)
    {
        OSPRayRenderer* osprayRenderer =
            dynamic_cast<OSPRayRenderer*>(renderer.get());
        ospSetData(osprayRenderer->impl(), "materials", _ospMaterialData);
        ospCommit(osprayRenderer->impl());
    }
}

void OSPRayScene::commitTransferFunctionData()
{
    if (_ospVolume)
    {
        std::vector<float> opacityValues;
        opacityValues.resize(_transferFunction.getDiffuseColors().size());
        for (size_t i = 0; i < _transferFunction.getDiffuseColors().size(); ++i)
            opacityValues[i] = _transferFunction.getDiffuseColors()[i][3];
        OSPData opacityValuesData =
            ospNewData(opacityValues.size(), OSP_FLOAT, opacityValues.data());
        ospSetData(_ospTransferFunction, "opacities", opacityValuesData);

        ospcommon::vec2f valueRange{_transferFunction.getValuesRange()[0],
                                    _transferFunction.getValuesRange()[1]};
        ospSet2f(_ospTransferFunction, "valueRange", valueRange.x,
                 valueRange.y);

        const auto& vol = _scene.getVolume();
        ospSet1i(_ospTransferFunction, "preIntegration",
                 vol.getPreintegration());

        ospCommit(_ospTransferFunction);
        return;
    }

    for (const auto& renderer : _renderers)
    {
        OSPRayRenderer* osprayRenderer =
            dynamic_cast<OSPRayRenderer*>(renderer.get());

        // Transfer function Diffuse colors
        _ospTransferFunctionDiffuseData =
            ospNewData(_transferFunction.getDiffuseColors().size(), OSP_FLOAT4,
                       _transferFunction.getDiffuseColors().data(),
                       _getOSPDataFlags());
        ospCommit(_ospTransferFunctionDiffuseData);
        ospSetData(osprayRenderer->impl(), "transferFunctionDiffuseData",
                   _ospTransferFunctionDiffuseData);

        // Transfer function emission data
        _ospTransferFunctionEmissionData =
            ospNewData(_transferFunction.getEmissionIntensities().size(),
                       OSP_FLOAT3,
                       _transferFunction.getEmissionIntensities().data(),
                       _getOSPDataFlags());
        ospCommit(_ospTransferFunctionEmissionData);
        ospSetData(osprayRenderer->impl(), "transferFunctionEmissionData",
                   _ospTransferFunctionEmissionData);

        // Transfer function size
        ospSet1i(osprayRenderer->impl(), "transferFunctionSize",
                 _transferFunction.getDiffuseColors().size());

        // Transfer function range
        ospSet1f(osprayRenderer->impl(), "transferFunctionMinValue",
                 _transferFunction.getValuesRange().x());
        ospSet1f(osprayRenderer->impl(), "transferFunctionRange",
                 _transferFunction.getValuesRange().y() -
                     _transferFunction.getValuesRange().x());
    }
}

void OSPRayScene::commitVolumeData()
{
    VolumeHandlerPtr volumeHandler = getVolumeHandler();
    if (_parametersManager.getRenderingParameters().getRenderer() ==
            RendererType::scivis ||
        !volumeHandler)
    {
        _commitBrickedVolumeData();
        return;
    }

    if (!volumeHandler)
        return;

    const float timestamp =
        _parametersManager.getSceneParameters().getTimestamp();
    volumeHandler->setTimestamp(timestamp);
    void* data = volumeHandler->getData();
    if (data)
    {
        for (const auto& renderer : _renderers)
        {
            OSPRayRenderer* osprayRenderer =
                dynamic_cast<OSPRayRenderer*>(renderer.get());

            const size_t size = volumeHandler->getSize();

            _ospVolumeData =
                ospNewData(size, OSP_UCHAR, data, _getOSPDataFlags());
            ospCommit(_ospVolumeData);
            ospSetData(osprayRenderer->impl(), "volumeData", _ospVolumeData);

            const Vector3ui& dimensions = volumeHandler->getDimensions();
            ospSet3i(osprayRenderer->impl(), "volumeDimensions", dimensions.x(),
                     dimensions.y(), dimensions.z());

            const Vector3f& elementSpacing =
                _parametersManager.getVolumeParameters().getElementSpacing();
            ospSet3f(osprayRenderer->impl(), "volumeElementSpacing",
                     elementSpacing.x(), elementSpacing.y(),
                     elementSpacing.z());

            const Vector3f& offset =
                _parametersManager.getVolumeParameters().getOffset();
            ospSet3f(osprayRenderer->impl(), "volumeOffset", offset.x(),
                     offset.y(), offset.z());

            const float epsilon = volumeHandler->getEpsilon(
                elementSpacing,
                _parametersManager.getVolumeParameters().getSamplesPerRay());
            ospSet1f(osprayRenderer->impl(), "volumeEpsilon", epsilon);
        }
    }
}

void OSPRayScene::commitSimulationData()
{
    if (!_simulationHandler)
        return;

    const float timestamp =
        _parametersManager.getSceneParameters().getTimestamp();

    if (_simulationHandler->getTimestamp() == timestamp)
        return;

    _simulationHandler->setTimestamp(timestamp);
    auto frameData = _simulationHandler->getFrameData();

    if (!frameData)
        return;

    for (const auto& renderer : _renderers)
    {
        OSPRayRenderer* osprayRenderer =
            dynamic_cast<OSPRayRenderer*>(renderer.get());

        _ospSimulationData =
            ospNewData(_simulationHandler->getFrameSize(), OSP_FLOAT, frameData,
                       _getOSPDataFlags());
        ospCommit(_ospSimulationData);
        ospSetData(osprayRenderer->impl(), "simulationData",
                   _ospSimulationData);
        ospSet1i(osprayRenderer->impl(), "simulationDataSize",
                 _simulationHandler->getFrameSize());
        ospCommit(osprayRenderer->impl());
    }
}

OSPTexture2D OSPRayScene::_createTexture2D(const std::string& textureName)
{
    if (_ospTextures.find(textureName) != _ospTextures.end())
        return _ospTextures[textureName];

    Texture2DPtr texture = _textures[textureName];
    if (!texture)
    {
        BRAYNS_WARN << "Texture " << textureName << " is not in the cache"
                    << std::endl;
        return nullptr;
    }

    OSPTextureFormat type = OSP_TEXTURE_R8; // smallest valid type as default
    if (texture->getDepth() == 1)
    {
        if (texture->getNbChannels() == 3)
            type = OSP_TEXTURE_SRGB;
        if (texture->getNbChannels() == 4)
            type = OSP_TEXTURE_SRGBA;
    }
    else if (texture->getDepth() == 4)
    {
        if (texture->getNbChannels() == 3)
            type = OSP_TEXTURE_RGB32F;
        if (texture->getNbChannels() == 4)
            type = OSP_TEXTURE_RGBA32F;
    }

    BRAYNS_DEBUG << "Creating OSPRay texture from " << textureName << " :"
                 << texture->getWidth() << "x" << texture->getHeight() << "x"
                 << (int)type << std::endl;

    osp::vec2i texSize{int(texture->getWidth()), int(texture->getHeight())};
    OSPTexture2D ospTexture =
        ospNewTexture2D(texSize, type, texture->getRawData(), 0);

    assert(ospTexture);
    ospCommit(ospTexture);

    _ospTextures[textureName] = ospTexture;

    return ospTexture;
}

void OSPRayScene::saveSceneToCacheFile()
{
    _saveCacheFile();
}

bool OSPRayScene::isVolumeSupported(const std::string& volumeFile) const
{
    return boost::algorithm::ends_with(volumeFile, ".raw");
}

bool OSPRayScene::isBrickedVolumeSupported(const std::string& volumeFile) const
{
    return BrickedVolumeHandler::isVolumeSupported(volumeFile);
}

uint32_t OSPRayScene::_getOSPDataFlags()
{
    return _parametersManager.getGeometryParameters().getMemoryMode() ==
                   MemoryMode::shared
               ? OSP_DATA_SHARED_BUFFER
               : 0;
}
}
