import numpy as np
import matplotlib.pyplot as plt
from matplotlib.widgets import Slider

# ----------------------------
# config
# ----------------------------
L = [6.0, 5.0, 3.0, 2.5]

JOINT_LIMITS = [
    (0,    0,   0),    # joint 1 fixed
    (-30,  90, -10),   # joint 2
    (0,   110,  10),   # joint 3
    (-10,  90,  10),   # joint 4
]

RAY_LEN = 30.0
WRIST_TOL = 0.15
CCD_ITERS = 25

_last_solution = [j[2] for j in JOINT_LIMITS]
_last_dist = sum(L) * 0.7


# ----------------------------
# kinematics
# ----------------------------
def fk_points(joint_deg):
    x, y = 0.0, 0.0
    pts = [(x, y)]
    theta = 0.0
    for Li, ai in zip(L, joint_deg):
        theta += np.deg2rad(ai)
        x += Li * np.cos(theta)
        y += Li * np.sin(theta)
        pts.append((x, y))
    return np.array(pts)

def clamp(v, mn, mx):
    return mn if v < mn else mx if v > mx else v

def wrap180(d):
    return (d + 180) % 360 - 180


# ----------------------------
# CCD IK for wrist
# ----------------------------
def wrist_points(j2, j3):
    j = [0, j2, j3, 0]
    pts = fk_points(j)
    return pts[:4]  # p0 p1 p2 wrist

def solve_wrist(target, seed_j2, seed_j3):
    mn2, mx2, _ = JOINT_LIMITS[1]
    mn3, mx3, _ = JOINT_LIMITS[2]

    j2 = clamp(seed_j2, mn2, mx2)
    j3 = clamp(seed_j3, mn3, mx3)

    for _ in range(CCD_ITERS):
        pts = wrist_points(j2, j3)
        p0, p1, p2, wrist = pts

        if np.linalg.norm(wrist - target) < WRIST_TOL:
            break

        # joint3
        v1 = wrist - p2
        v2 = target - p2
        if np.linalg.norm(v1) > 1e-6 and np.linalg.norm(v2) > 1e-6:
            a1 = np.arctan2(v1[1], v1[0])
            a2 = np.arctan2(v2[1], v2[0])
            j3 += wrap180(np.rad2deg(a2 - a1))
            j3 = clamp(j3, mn3, mx3)

        pts = wrist_points(j2, j3)
        p0, p1, p2, wrist = pts

        if np.linalg.norm(wrist - target) < WRIST_TOL:
            break

        # joint2
        v1 = wrist - p1
        v2 = target - p1
        if np.linalg.norm(v1) > 1e-6 and np.linalg.norm(v2) > 1e-6:
            a1 = np.arctan2(v1[1], v1[0])
            a2 = np.arctan2(v2[1], v2[0])
            j2 += wrap180(np.rad2deg(a2 - a1))
            j2 = clamp(j2, mn2, mx2)

    pts = wrist_points(j2, j3)
    err = np.linalg.norm(pts[-1] - target)
    return j2, j3, err


# ----------------------------
# main solver
# ----------------------------
def solve_to_distal(distal_angle_deg):
    global _last_solution, _last_dist

    distal_abs = np.deg2rad(distal_angle_deg)
    ray_ang = distal_abs - np.pi/2
    ray_dir = np.array([np.cos(ray_ang), np.sin(ray_ang)])
    distal_dir = np.array([np.cos(distal_abs), np.sin(distal_abs)])

    def wrist_target(dist):
        tip = ray_dir * dist
        return tip - distal_dir * L[3]

    seed = _last_solution

    for dist in np.linspace(sum(L), 0, 160):
        w_tgt = wrist_target(dist)

        j2, j3, err = solve_wrist(w_tgt, seed[1], seed[2])
        if err > WRIST_TOL:
            continue

        j1 = 0
        j4 = distal_angle_deg - (j1 + j2 + j3)

        mn4, mx4, _ = JOINT_LIMITS[3]
        if not (mn4 <= j4 <= mx4):
            continue

        sol = [j1, j2, j3, j4]
        _last_solution = sol
        _last_dist = dist
        return sol

    return _last_solution


# ----------------------------
# plotting
# ----------------------------
fig, ax = plt.subplots()
plt.subplots_adjust(bottom=0.25)

(chain_line,) = ax.plot([0],[0],"-o",linewidth=2)
(ray_line,) = ax.plot([0],[0],linewidth=2)
(distal_line,) = ax.plot([0],[0],linewidth=3)

ax.set_aspect("equal")
ax.set_xlim(-25,25)
ax.set_ylim(-25,25)
ax.grid()

# slider for distal angle
ax_slider = fig.add_axes([0.2,0.1,0.6,0.04])
distal_slider = Slider(ax_slider,"distal angle",-180,180,valinit=30)


def update(_):
    distal_angle = distal_slider.val
    j = solve_to_distal(distal_angle)

    pts = fk_points(j)
    chain_line.set_data(pts[:,0], pts[:,1])

    tip = pts[-1]
    base = pts[-2]
    distal_line.set_data([base[0],tip[0]],[base[1],tip[1]])

    distal_abs = np.deg2rad(distal_angle)
    ray_ang = distal_abs - np.pi/2
    dx, dy = np.cos(ray_ang), np.sin(ray_ang)

    ray_line.set_data([tip[0], tip[0]-RAY_LEN*dx],
                      [tip[1], tip[1]-RAY_LEN*dy])

    fig.canvas.draw_idle()


distal_slider.on_changed(update)
update(0)

plt.show()