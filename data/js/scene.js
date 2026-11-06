/* Scene geometry designer + console cell viewer (custom Canvas, no three.js). */
(function (global) {
    "use strict";

    var DEFAULT_FLOOR_COLOR = "#6a8fad";
    var DEFAULT_WALL_THICKNESS = 0.04;
    var DEFAULT_WALL_HEIGHT = 0.15;

    function defaultPolygon(sceneSize) {
        var size = sceneSize > 0 ? sceneSize : 1;
        var half = size * 0.35;
        // CCW in XZ when viewed from +Y so the floor normal faces up.
        return [
            { x: -half, z: -half },
            { x: -half, z: half },
            { x: half, z: half },
            { x: half, z: -half }
        ];
    }

    function wallsAlongPolygon(polygon) {
        var walls = [];
        if (!polygon || polygon.length < 2) {
            return walls;
        }
        var index;
        for (index = 0; index < polygon.length; index++) {
            var a = polygon[index];
            var b = polygon[(index + 1) % polygon.length];
            walls.push({ x1: a.x, z1: a.z, x2: b.x, z2: b.z });
        }
        return walls;
    }

    function polygonFromWalls(walls) {
        if (!walls || walls.length < 3) {
            return null;
        }
        var points = [];
        var index;
        for (index = 0; index < walls.length; index++) {
            points.push({ x: walls[index].x1, z: walls[index].z1 });
        }
        return points;
    }

    function defaultFloorRect(sceneSize) {
        var polygon = defaultPolygon(sceneSize);
        return {
            y: 0,
            height: 0.3,
            color: DEFAULT_FLOOR_COLOR,
            polygon: polygon,
            walls: wallsAlongPolygon(polygon),
            windows: [],
            doors: []
        };
    }

    function normalizeFloor(floor, sceneSize) {
        if (!floor || typeof floor !== "object") {
            return defaultFloorRect(sceneSize);
        }
        if (!floor.color || typeof floor.color !== "string") {
            floor.color = DEFAULT_FLOOR_COLOR;
        }
        if (!Array.isArray(floor.walls)) {
            floor.walls = [];
        }
        if (!Array.isArray(floor.windows)) {
            floor.windows = [];
        }
        if (!Array.isArray(floor.doors)) {
            floor.doors = [];
        }
        if (!Array.isArray(floor.polygon) || floor.polygon.length < 3) {
            floor.polygon = polygonFromWalls(floor.walls) || defaultPolygon(sceneSize);
        }
        return floor;
    }

    function pointsNearEqual(ax, az, bx, bz, epsilon) {
        var eps = epsilon == null ? 1e-6 : epsilon;
        var dx = ax - bx;
        var dz = az - bz;
        return (dx * dx + dz * dz) <= (eps * eps);
    }

    function hexToRgb(hex) {
        var value = String(hex || DEFAULT_FLOOR_COLOR).replace("#", "");
        if (value.length === 3) {
            value = value[0] + value[0] + value[1] + value[1] + value[2] + value[2];
        }
        if (value.length !== 6) {
            return { r: 106, g: 143, b: 173 };
        }
        var r = parseInt(value.slice(0, 2), 16);
        var g = parseInt(value.slice(2, 4), 16);
        var b = parseInt(value.slice(4, 6), 16);
        if (isNaN(r) || isNaN(g) || isNaN(b)) {
            return { r: 106, g: 143, b: 173 };
        }
        return { r: r, g: g, b: b };
    }

    function hexToRgba(hex, alpha) {
        var rgb = hexToRgb(hex);
        return "rgba(" + rgb.r + "," + rgb.g + "," + rgb.b + "," + alpha + ")";
    }

    function shadeRgba(hex, shade, alpha) {
        var rgb = hexToRgb(hex);
        var factor = Math.max(0.15, Math.min(1.2, shade));
        return "rgba("
            + Math.round(Math.min(255, rgb.r * factor)) + ","
            + Math.round(Math.min(255, rgb.g * factor)) + ","
            + Math.round(Math.min(255, rgb.b * factor)) + ","
            + alpha + ")";
    }

    function moveSharedWallCorner(floor, fromX, fromZ, toX, toZ) {
        if (!floor || !Array.isArray(floor.walls)) {
            return;
        }
        var epsilon = 1e-5;
        floor.walls.forEach(function (seg) {
            if (pointsNearEqual(seg.x1, seg.z1, fromX, fromZ, epsilon)) {
                seg.x1 = toX;
                seg.z1 = toZ;
            }
            if (pointsNearEqual(seg.x2, seg.z2, fromX, fromZ, epsilon)) {
                seg.x2 = toX;
                seg.z2 = toZ;
            }
        });
    }

    function axisLockPoint(origin, point, shiftHeld) {
        if (!shiftHeld || !origin || !point) {
            return point;
        }
        var dx = Math.abs(point.x - origin.x);
        var dz = Math.abs(point.z - origin.z);
        if (dx >= dz) {
            return { x: point.x, z: origin.z };
        }
        return { x: origin.x, z: point.z };
    }

    function smoothToward(current, target, factor) {
        var t = factor == null ? 0.55 : factor;
        if (t < 0) {
            t = 0;
        } else if (t > 1) {
            t = 1;
        }
        return {
            x: current.x + (target.x - current.x) * t,
            z: current.z + (target.z - current.z) * t
        };
    }

    function defaultModel() {
        return {
            sceneSize: 1.0,
            wallHeight: DEFAULT_WALL_HEIGHT,
            wallThickness: DEFAULT_WALL_THICKNESS,
            floors: [defaultFloorRect(1.0)]
        };
    }

    function cloneJson(value) {
        return JSON.parse(JSON.stringify(value));
    }

    function clamp01(value) {
        if (value < 0) {
            return 0;
        }
        if (value > 1) {
            return 1;
        }
        return value;
    }

    function distPointToSeg(px, pz, seg) {
        var ax = seg.x1;
        var az = seg.z1;
        var bx = seg.x2;
        var bz = seg.z2;
        var abx = bx - ax;
        var abz = bz - az;
        var len2 = abx * abx + abz * abz;
        if (len2 < 1e-12) {
            var dx0 = px - ax;
            var dz0 = pz - az;
            return Math.sqrt(dx0 * dx0 + dz0 * dz0);
        }
        var t = ((px - ax) * abx + (pz - az) * abz) / len2;
        if (t < 0) {
            t = 0;
        } else if (t > 1) {
            t = 1;
        }
        var qx = ax + abx * t;
        var qz = az + abz * t;
        var dx = px - qx;
        var dz = pz - qz;
        return Math.sqrt(dx * dx + dz * dz);
    }

    function projectOnSeg(px, pz, seg) {
        var ax = seg.x1;
        var az = seg.z1;
        var bx = seg.x2;
        var bz = seg.z2;
        var abx = bx - ax;
        var abz = bz - az;
        var len2 = abx * abx + abz * abz;
        if (len2 < 1e-12) {
            return { x: ax, z: az, t: 0 };
        }
        var t = ((px - ax) * abx + (pz - az) * abz) / len2;
        if (t < 0) {
            t = 0;
        } else if (t > 1) {
            t = 1;
        }
        return { x: ax + abx * t, z: az + abz * t, t: t };
    }

    function projectOrbit(point, yaw, pitch, distance, width, height, pixelsPerUnit) {
        var cy = Math.cos(yaw);
        var sy = Math.sin(yaw);
        var cp = Math.cos(pitch);
        var sp = Math.sin(pitch);
        var x = point.x;
        var y = point.y;
        var z = point.z;
        // Orbit yaw around Y, then pitch so positive pitch looks down from above.
        var rx = x * cy + z * sy;
        var rz = -x * sy + z * cy;
        var ry = y * cp + rz * sp;
        var rz2 = -y * sp + rz * cp;
        var depth = distance + rz2;
        if (depth < 0.05) {
            depth = 0.05;
        }
        var scale = (pixelsPerUnit > 0 ? pixelsPerUnit : Math.min(width, height) * 0.42) / depth;
        return {
            x: width * 0.5 + rx * scale,
            y: height * 0.58 - ry * scale,
            depth: depth
        };
    }

    function sceneBounds(model) {
        var minX = 0;
        var maxX = 0;
        var minY = 0;
        var maxY = 0;
        var minZ = 0;
        var maxZ = 0;
        var hasPoint = false;
        function includePoint(x, y, z) {
            if (!hasPoint) {
                minX = maxX = x;
                minY = maxY = y;
                minZ = maxZ = z;
                hasPoint = true;
                return;
            }
            if (x < minX) { minX = x; }
            if (x > maxX) { maxX = x; }
            if (y < minY) { minY = y; }
            if (y > maxY) { maxY = y; }
            if (z < minZ) { minZ = z; }
            if (z > maxZ) { maxZ = z; }
        }
        var wallHeight = model.wallHeight || DEFAULT_WALL_HEIGHT;
        (model.floors || []).forEach(function (floor) {
            var baseY = floor.y || 0;
            (floor.polygon || []).forEach(function (corner) {
                includePoint(corner.x, baseY, corner.z);
                includePoint(corner.x, baseY + wallHeight, corner.z);
            });
            ["walls", "windows", "doors"].forEach(function (listName) {
                (floor[listName] || []).forEach(function (seg) {
                    includePoint(seg.x1, baseY, seg.z1);
                    includePoint(seg.x2, baseY, seg.z2);
                    includePoint(seg.x1, baseY + wallHeight, seg.z1);
                    includePoint(seg.x2, baseY + wallHeight, seg.z2);
                });
            });
        });
        if (!hasPoint) {
            var half = (model.sceneSize || 1) * 0.35;
            return { minX: -half, maxX: half, minY: 0, maxY: wallHeight, minZ: -half, maxZ: half };
        }
        return { minX: minX, maxX: maxX, minY: minY, maxY: maxY, minZ: minZ, maxZ: maxZ };
    }

    function viewProjectionParams(model, width, height, yaw, pitch) {
        var bounds = sceneBounds(model);
        var cx = (bounds.minX + bounds.maxX) * 0.5;
        var cy = (bounds.minY + bounds.maxY) * 0.5;
        var cz = (bounds.minZ + bounds.maxZ) * 0.5;
        var radius = Math.max(
            bounds.maxX - bounds.minX,
            bounds.maxY - bounds.minY,
            bounds.maxZ - bounds.minZ,
            (model.sceneSize || 1) * 0.4
        ) * 0.5;
        var distance = Math.max(1.2, radius * 2.35);
        var sample = [
            { x: bounds.minX - cx, y: bounds.minY - cy, z: bounds.minZ - cz },
            { x: bounds.maxX - cx, y: bounds.minY - cy, z: bounds.minZ - cz },
            { x: bounds.minX - cx, y: bounds.maxY - cy, z: bounds.minZ - cz },
            { x: bounds.maxX - cx, y: bounds.maxY - cy, z: bounds.minZ - cz },
            { x: bounds.minX - cx, y: bounds.minY - cy, z: bounds.maxZ - cz },
            { x: bounds.maxX - cx, y: bounds.minY - cy, z: bounds.maxZ - cz },
            { x: bounds.minX - cx, y: bounds.maxY - cy, z: bounds.maxZ - cz },
            { x: bounds.maxX - cx, y: bounds.maxY - cy, z: bounds.maxZ - cz }
        ];
        var maxSpan = 0.001;
        sample.forEach(function (point) {
            var projected = projectOrbit(point, yaw, pitch, distance, width, height, 1);
            var dx = projected.x - width * 0.5;
            var dy = projected.y - height * 0.58;
            var span = Math.sqrt(dx * dx + dy * dy);
            if (span > maxSpan) {
                maxSpan = span;
            }
        });
        var targetRadiusPx = Math.min(width, height) * 0.38;
        return {
            centerX: cx,
            centerY: cy,
            centerZ: cz,
            distance: distance,
            pixelsPerUnit: targetRadiusPx / maxSpan
        };
    }

    function cross(ax, ay, az, bx, by, bz) {
        return {
            x: ay * bz - az * by,
            y: az * bx - ax * bz,
            z: ax * by - ay * bx
        };
    }

    function normalize3(vector) {
        var len = Math.sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
        if (len < 1e-9) {
            return { x: 0, y: 1, z: 0 };
        }
        return { x: vector.x / len, y: vector.y / len, z: vector.z / len };
    }

    function faceNormal(points) {
        if (!points || points.length < 3) {
            return { x: 0, y: 1, z: 0 };
        }
        return normalize3(cross(
            points[1].x - points[0].x,
            points[1].y - points[0].y,
            points[1].z - points[0].z,
            points[2].x - points[0].x,
            points[2].y - points[0].y,
            points[2].z - points[0].z
        ));
    }

    function averageDepth(projected) {
        var sum = 0;
        var index;
        for (index = 0; index < projected.length; index++) {
            sum += projected[index].depth;
        }
        return sum / projected.length;
    }

    function wallLength(seg) {
        var dx = seg.x2 - seg.x1;
        var dz = seg.z2 - seg.z1;
        return Math.sqrt(dx * dx + dz * dz);
    }

    function pointOnWall(seg, t) {
        return {
            x: seg.x1 + (seg.x2 - seg.x1) * t,
            z: seg.z1 + (seg.z2 - seg.z1) * t
        };
    }

    function endpointsNear(ax, az, bx, bz, epsilon) {
        return pointsNearEqual(ax, az, bx, bz, epsilon == null ? 1e-4 : epsilon);
    }

    function wallConnectedAt(walls, wallIndex, end, epsilon) {
        var wall = walls[wallIndex];
        if (!wall) {
            return false;
        }
        var x = end === 1 ? wall.x1 : wall.x2;
        var z = end === 1 ? wall.z1 : wall.z2;
        var index;
        for (index = 0; index < walls.length; index++) {
            if (index === wallIndex) {
                continue;
            }
            var other = walls[index];
            if (endpointsNear(x, z, other.x1, other.z1, epsilon)
                || endpointsNear(x, z, other.x2, other.z2, epsilon)) {
                return true;
            }
        }
        return false;
    }

    // Thick wall box with optional end trims so joined corners do not X-overlap.
    function wallBoxFaces(seg, baseY, wallHeight, thickness, colorHex, alpha, options) {
        var faces = [];
        options = options || {};
        var dx = seg.x2 - seg.x1;
        var dz = seg.z2 - seg.z1;
        var len = Math.sqrt(dx * dx + dz * dz);
        if (len < 1e-9) {
            return faces;
        }
        var ux = dx / len;
        var uz = dz / len;
        var nx = -uz;
        var nz = ux;
        var half = Math.max(0.004, thickness * 0.5);
        var trimStart = options.trimStart || 0;
        var trimEnd = options.trimEnd || 0;
        if (trimStart + trimEnd > len * 0.85) {
            var scale = (len * 0.85) / (trimStart + trimEnd);
            trimStart *= scale;
            trimEnd *= scale;
        }
        var x1 = seg.x1 + ux * trimStart;
        var z1 = seg.z1 + uz * trimStart;
        var x2 = seg.x2 - ux * trimEnd;
        var z2 = seg.z2 - uz * trimEnd;
        var y0 = baseY;
        var y1 = baseY + wallHeight;
        var aOut = { x: x1 + nx * half, y: y0, z: z1 + nz * half };
        var bOut = { x: x2 + nx * half, y: y0, z: z2 + nz * half };
        var bIn = { x: x2 - nx * half, y: y0, z: z2 - nz * half };
        var aIn = { x: x1 - nx * half, y: y0, z: z1 - nz * half };
        var aOutTop = { x: aOut.x, y: y1, z: aOut.z };
        var bOutTop = { x: bOut.x, y: y1, z: bOut.z };
        var bInTop = { x: bIn.x, y: y1, z: bIn.z };
        var aInTop = { x: aIn.x, y: y1, z: aIn.z };
        faces.push({ points: [aOut, bOut, bOutTop, aOutTop], color: colorHex, alpha: alpha });
        faces.push({ points: [bIn, aIn, aInTop, bInTop], color: colorHex, alpha: alpha });
        if (!options.skipStartCap) {
            faces.push({ points: [aIn, aOut, aOutTop, aInTop], color: colorHex, alpha: alpha });
        }
        if (!options.skipEndCap) {
            faces.push({ points: [bOut, bIn, bInTop, bOutTop], color: colorHex, alpha: alpha });
        }
        faces.push({ points: [aOutTop, bOutTop, bInTop, aInTop], color: colorHex, alpha: alpha });
        return faces;
    }

    function cornerPostFaces(x, z, baseY, wallHeight, thickness, colorHex) {
        var half = Math.max(0.004, thickness * 0.5);
        return wallBoxFaces(
            { x1: x - half, z1: z, x2: x + half, z2: z },
            baseY,
            wallHeight,
            thickness,
            colorHex,
            1,
            { trimStart: 0, trimEnd: 0 }
        );
    }

    function projectOpeningOntoWall(wall, opening, maxDist) {
        var len = wallLength(wall);
        if (len < 1e-9) {
            return null;
        }
        var a = projectOnSeg(opening.x1, opening.z1, wall);
        var b = projectOnSeg(opening.x2, opening.z2, wall);
        var distA = distPointToSeg(opening.x1, opening.z1, wall);
        var distB = distPointToSeg(opening.x2, opening.z2, wall);
        if (distA > maxDist || distB > maxDist) {
            return null;
        }
        var t0 = Math.min(a.t, b.t);
        var t1 = Math.max(a.t, b.t);
        if (t0 < 0) {
            t0 = 0;
        }
        if (t1 > 1) {
            t1 = 1;
        }
        if (t1 - t0 < 0.02) {
            return null;
        }
        return { t0: t0, t1: t1 };
    }

    function wallSlab(wall, t0, t1, baseY, height, thickness, colorHex, alpha, options) {
        if (height <= 1e-6 || t1 - t0 <= 1e-4) {
            return [];
        }
        var a = pointOnWall(wall, t0);
        var b = pointOnWall(wall, t1);
        return wallBoxFaces(
            { x1: a.x, z1: a.z, x2: b.x, z2: b.z },
            baseY,
            height,
            thickness,
            colorHex,
            alpha,
            options || {}
        );
    }

    function wallFacesWithOpenings(wall, openings, baseY, wallHeight, thickness, joinOptions) {
        var faces = [];
        var wallColor = "#cfd6e0";
        var half = Math.max(0.004, thickness * 0.5);
        var join = joinOptions || {};
        var trimStart = join.connectedStart ? half : 0;
        var trimEnd = join.connectedEnd ? half : 0;
        var skipStartCap = !!join.connectedStart;
        var skipEndCap = !!join.connectedEnd;
        var cuts = [];
        openings.forEach(function (opening) {
            var span = projectOpeningOntoWall(wall, opening.seg, Math.max(thickness * 2, 0.04));
            if (!span) {
                return;
            }
            cuts.push({
                t0: span.t0,
                t1: span.t1,
                color: opening.color,
                y0: opening.y0,
                y1: opening.y1
            });
        });
        cuts.sort(function (left, right) {
            return left.t0 - right.t0;
        });

        function solidSpan(t0, t1, y0, height, color) {
            if (t1 <= t0 || height <= 1e-6) {
                return;
            }
            var options = {
                trimStart: 0,
                trimEnd: 0,
                skipStartCap: true,
                skipEndCap: true
            };
            // Apply join trims only on the full-length ends of the original wall.
            if (t0 <= 1e-4) {
                options.trimStart = trimStart;
                options.skipStartCap = skipStartCap;
            }
            if (t1 >= 1 - 1e-4) {
                options.trimEnd = trimEnd;
                options.skipEndCap = skipEndCap;
            }
            faces = faces.concat(wallSlab(wall, t0, t1, y0, height, thickness, color, 1, options));
        }

        var cursor = 0;
        if (!cuts.length) {
            solidSpan(0, 1, baseY, wallHeight, wallColor);
            return faces;
        }
        cuts.forEach(function (cut) {
            var t0 = Math.max(cursor, cut.t0);
            var t1 = Math.max(t0, cut.t1);
            if (t0 > cursor + 1e-4) {
                solidSpan(cursor, t0, baseY, wallHeight, wallColor);
            }
            if (cut.y0 > baseY + 1e-4) {
                solidSpan(t0, t1, baseY, cut.y0 - baseY, wallColor);
            }
            if (cut.y1 < baseY + wallHeight - 1e-4) {
                solidSpan(t0, t1, cut.y1, (baseY + wallHeight) - cut.y1, wallColor);
            }
            // Thin colored jambs only — never fill the opening.
            var openingLen = (t1 - t0) * wallLength(wall);
            var jamb = Math.min(0.005, openingLen * 0.1);
            if (jamb * 2.2 < openingLen) {
                var dt = jamb / wallLength(wall);
                faces = faces.concat(wallSlab(
                    wall, t0, t0 + dt, cut.y0, cut.y1 - cut.y0, thickness, cut.color, 1,
                    { skipStartCap: true, skipEndCap: true }
                ));
                faces = faces.concat(wallSlab(
                    wall, t1 - dt, t1, cut.y0, cut.y1 - cut.y0, thickness, cut.color, 1,
                    { skipStartCap: true, skipEndCap: true }
                ));
            }
            cursor = Math.max(cursor, t1);
        });
        if (cursor < 1 - 1e-4) {
            solidSpan(cursor, 1, baseY, wallHeight, wallColor);
        }
        return faces;
    }

    function closedLoopFromWalls(walls) {
        if (!walls || walls.length < 3) {
            return null;
        }
        var epsilon = 1e-4;
        var used = walls.map(function () { return false; });
        var points = [{ x: walls[0].x1, z: walls[0].z1 }, { x: walls[0].x2, z: walls[0].z2 }];
        used[0] = true;
        var guard;
        for (guard = 0; guard < walls.length + 2; guard++) {
            var tip = points[points.length - 1];
            var found = -1;
            var reverse = false;
            var index;
            for (index = 0; index < walls.length; index++) {
                if (used[index]) {
                    continue;
                }
                if (endpointsNear(tip.x, tip.z, walls[index].x1, walls[index].z1, epsilon)) {
                    found = index;
                    reverse = false;
                    break;
                }
                if (endpointsNear(tip.x, tip.z, walls[index].x2, walls[index].z2, epsilon)) {
                    found = index;
                    reverse = true;
                    break;
                }
            }
            if (found < 0) {
                break;
            }
            used[found] = true;
            var next = reverse
                ? { x: walls[found].x1, z: walls[found].z1 }
                : { x: walls[found].x2, z: walls[found].z2 };
            points.push(next);
        }
        var allUsed = used.every(function (flag) { return flag; });
        if (!allUsed || points.length < 4) {
            return null;
        }
        var first = points[0];
        var last = points[points.length - 1];
        if (!endpointsNear(first.x, first.z, last.x, last.z, epsilon)) {
            return null;
        }
        points.pop();
        return points;
    }

    function makeFloorFace(points2d, baseY, colorHex) {
        if (!points2d || points2d.length < 3) {
            return null;
        }
        var floorPoints = points2d.map(function (corner) {
            return { x: corner.x, y: baseY + 0.001, z: corner.z };
        });
        if (faceNormal(floorPoints).y < 0) {
            floorPoints = floorPoints.slice().reverse();
        }
        return {
            points: floorPoints,
            color: colorHex || DEFAULT_FLOOR_COLOR,
            alpha: 1,
            forceDraw: true
        };
    }

    function buildSceneFaces(model) {
        var faces = [];
        var wallHeight = model.wallHeight || DEFAULT_WALL_HEIGHT;
        var thickness = model.wallThickness || DEFAULT_WALL_THICKNESS;
        (model.floors || []).forEach(function (floor) {
            var baseY = floor.y || 0;
            var walls = floor.walls || [];
            var loop = closedLoopFromWalls(walls);
            var floorFace = makeFloorFace(floor.polygon, baseY, floor.color);
            // Prefer a closed wall loop for the visible floor plate so the room is never empty.
            if (loop) {
                floorFace = makeFloorFace(loop, baseY, floor.color) || floorFace;
            }
            if (floorFace) {
                faces.push(floorFace);
            }

            var openings = [];
            (floor.windows || []).forEach(function (seg) {
                openings.push({
                    seg: seg,
                    color: "#4fc3f7",
                    y0: baseY + wallHeight * 0.28,
                    y1: baseY + wallHeight * 0.78
                });
            });
            (floor.doors || []).forEach(function (seg) {
                openings.push({
                    seg: seg,
                    color: "#ffb74d",
                    y0: baseY,
                    y1: baseY + wallHeight * 0.92
                });
            });

            var cornerKeys = {};
            walls.forEach(function (seg, index) {
                var join = {
                    connectedStart: wallConnectedAt(walls, index, 1),
                    connectedEnd: wallConnectedAt(walls, index, 2)
                };
                faces = faces.concat(wallFacesWithOpenings(
                    seg, openings, baseY, wallHeight, thickness, join
                ));
                if (join.connectedStart) {
                    cornerKeys[seg.x1.toFixed(5) + "," + seg.z1.toFixed(5)] = {
                        x: seg.x1, z: seg.z1
                    };
                }
                if (join.connectedEnd) {
                    cornerKeys[seg.x2.toFixed(5) + "," + seg.z2.toFixed(5)] = {
                        x: seg.x2, z: seg.z2
                    };
                }
            });
            Object.keys(cornerKeys).forEach(function (key) {
                var corner = cornerKeys[key];
                faces = faces.concat(cornerPostFaces(
                    corner.x, corner.z, baseY, wallHeight, thickness, "#cfd6e0"
                ));
            });
        });
        return faces;
    }

    function paintFaces(ctx, faces, yaw, pitch, distance, width, height, pixelsPerUnit, center) {
        var light = normalize3({ x: 0.35, y: 0.9, z: 0.25 });
        var shift = center || { x: 0, y: 0, z: 0 };
        var prepared = faces.map(function (face) {
            var projected = face.points.map(function (point) {
                return projectOrbit({
                    x: point.x - shift.x,
                    y: point.y - shift.y,
                    z: point.z - shift.z
                }, yaw, pitch, distance, width, height, pixelsPerUnit);
            });
            // Screen-space cull (canvas Y grows downward).
            var facingScreen = 0;
            if (projected.length >= 3) {
                facingScreen = (projected[1].x - projected[0].x) * (projected[2].y - projected[0].y)
                    - (projected[1].y - projected[0].y) * (projected[2].x - projected[0].x);
            }
            if (!face.forceDraw && facingScreen > 0) {
                return null;
            }
            var normal = faceNormal(face.points);
            var facing = Math.max(0.2, Math.abs(
                normal.x * light.x + normal.y * light.y + normal.z * light.z
            ));
            var shade = 0.45 + 0.55 * facing;
            return {
                projected: projected,
                depth: averageDepth(projected),
                fill: shadeRgba(face.color, shade, face.alpha == null ? 1 : face.alpha),
                stroke: shadeRgba(face.color, shade * 0.7, 0.65)
            };
        }).filter(function (face) {
            return !!face;
        });
        prepared.sort(function (left, right) {
            return right.depth - left.depth;
        });
        prepared.forEach(function (face) {
            if (!face.projected.length) {
                return;
            }
            ctx.beginPath();
            face.projected.forEach(function (point, index) {
                if (index === 0) {
                    ctx.moveTo(point.x, point.y);
                } else {
                    ctx.lineTo(point.x, point.y);
                }
            });
            ctx.closePath();
            ctx.fillStyle = face.fill;
            ctx.fill();
            ctx.strokeStyle = face.stroke;
            ctx.lineWidth = 1;
            ctx.stroke();
        });
    }

    function SceneDesigner(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext("2d");
        this.model = defaultModel();
        this.floorIndex = 0;
        this.tool = "wall";
        this.viewMode = false;
        this.yaw = 0.85;
        this.pitch = 0.62;
        this.dragOrbit = false;
        this.dragHandle = null;
        this.dragCornerFrom = null;
        this.dragCornerOrigin = null;
        this.dragShiftHeld = false;
        this.lastDragClientX = 0;
        this.lastDragClientY = 0;
        this.lastX = 0;
        this.lastY = 0;
        this.onChange = null;
        this.selection = null;
        this.bindEvents();
        this.resize();
    }

    SceneDesigner.prototype.bindEvents = function () {
        var self = this;
        this.canvas.addEventListener("mousedown", function (event) {
            self.onPointerDown(event);
        });
        this.canvas.addEventListener("mousemove", function (event) {
            self.onPointerMove(event);
        });
        window.addEventListener("mousemove", function (event) {
            if (!self.dragHandle) {
                return;
            }
            self.onPointerMove(event);
        });
        window.addEventListener("mouseup", function (event) {
            self.dragOrbit = false;
            if (self.dragHandle) {
                self.finishHandleDrag(event);
            }
        });
        window.addEventListener("keydown", function (event) {
            if (event.key !== "Shift" || !self.dragHandle) {
                return;
            }
            self.dragShiftHeld = true;
            self.applyHandleDragAtClient(self.lastDragClientX, self.lastDragClientY, true);
        });
        window.addEventListener("keyup", function (event) {
            if (event.key !== "Shift" || !self.dragHandle) {
                return;
            }
            self.dragShiftHeld = false;
            self.applyHandleDragAtClient(self.lastDragClientX, self.lastDragClientY, false);
        });
        this.canvas.addEventListener("dblclick", function (event) {
            self.onDoubleClick(event);
        });
        this.canvas.addEventListener("wheel", function (event) {
            if (!self.viewMode) {
                return;
            }
            event.preventDefault();
            self.pitch = Math.max(0.28, Math.min(1.25, self.pitch + event.deltaY * 0.0015));
            self.draw();
        }, { passive: false });
    };

    SceneDesigner.prototype.resize = function () {
        var parent = this.canvas.parentElement;
        var width = parent ? parent.clientWidth : 640;
        var height = parent ? Math.max(320, parent.clientHeight || 420) : 420;
        this.canvas.width = Math.max(320, width);
        this.canvas.height = height;
        this.draw();
    };

    SceneDesigner.prototype.setModel = function (model) {
        this.model = model && model.floors ? cloneJson(model) : defaultModel();
        if (!this.model.floors.length) {
            this.model.floors = defaultModel().floors;
        }
        if (!(this.model.wallThickness > 0)) {
            this.model.wallThickness = DEFAULT_WALL_THICKNESS;
        }
        if (!(this.model.wallHeight > 0)) {
            this.model.wallHeight = DEFAULT_WALL_HEIGHT;
        }
        var sceneSize = this.model.sceneSize || 1;
        this.model.floors = this.model.floors.map(function (floor) {
            return normalizeFloor(floor, sceneSize);
        });
        this.floorIndex = 0;
        this.selection = null;
        this.dragHandle = null;
        this.dragCornerFrom = null;
        this.dragCornerOrigin = null;
        this.dragShiftHeld = false;
        this.draw();
    };

    SceneDesigner.prototype.getModel = function () {
        return cloneJson(this.model);
    };

    SceneDesigner.prototype.setTool = function (tool) {
        this.tool = tool || "wall";
        this.draw();
        return false;
    };

    SceneDesigner.prototype.applyTool = function (tool) {
        this.tool = tool || "wall";
        if (tool === "wall") {
            this.addWall();
            return true;
        }
        if ((tool === "window" || tool === "door") && this.placeOpeningOnSelectedWall(tool)) {
            this.draw();
            return true;
        }
        this.draw();
        return false;
    };

    SceneDesigner.prototype.addWall = function () {
        var size = this.model.sceneSize || 1;
        var length = size / 7;
        var floor = this.currentFloor();
        var offset = ((floor.walls || []).length % 6) * (length * 0.2);
        var midZ = offset - length * 0.4;
        floor.walls.push({
            x1: -length * 0.5 + offset * 0.25,
            z1: midZ,
            x2: length * 0.5 + offset * 0.25,
            z2: midZ
        });
        this.selection = { list: "walls", index: floor.walls.length - 1 };
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.setViewMode = function (enabled) {
        this.viewMode = !!enabled;
        this.dragHandle = null;
        this.draw();
    };

    SceneDesigner.prototype.toggleViewMode = function () {
        this.setViewMode(!this.viewMode);
        return this.viewMode;
    };

    SceneDesigner.prototype.addFloor = function () {
        var last = this.model.floors[this.model.floors.length - 1];
        var floor = defaultFloorRect(this.model.sceneSize || 1);
        floor.y = last ? (last.y + (last.height || 0.3)) : 0;
        if (last && last.color) {
            floor.color = last.color;
        }
        this.model.floors.push(floor);
        this.floorIndex = this.model.floors.length - 1;
        this.selection = { list: "polygon", index: 0 };
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.getCurrentFloorColor = function () {
        var floor = this.currentFloor();
        return floor && floor.color ? floor.color : DEFAULT_FLOOR_COLOR;
    };

    SceneDesigner.prototype.setCurrentFloorColor = function (color) {
        var floor = this.currentFloor();
        if (!floor) {
            return;
        }
        var next = String(color || "").trim();
        if (!/^#[0-9a-fA-F]{6}$/.test(next)) {
            return;
        }
        floor.color = next.toLowerCase();
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.getWallThickness = function () {
        return this.model.wallThickness > 0 ? this.model.wallThickness : DEFAULT_WALL_THICKNESS;
    };

    SceneDesigner.prototype.setWallThickness = function (value) {
        var next = Number(value);
        if (!isFinite(next)) {
            return;
        }
        if (next < 0.01) {
            next = 0.01;
        } else if (next > 0.25) {
            next = 0.25;
        }
        this.model.wallThickness = next;
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.getWallHeight = function () {
        return this.model.wallHeight > 0 ? this.model.wallHeight : DEFAULT_WALL_HEIGHT;
    };

    SceneDesigner.prototype.setWallHeight = function (value) {
        var next = Number(value);
        if (!isFinite(next)) {
            return;
        }
        if (next < 0.05) {
            next = 0.05;
        } else if (next > 1.5) {
            next = 1.5;
        }
        this.model.wallHeight = next;
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.placeOpeningOnSelectedWall = function (kind) {
        if (!this.selection || this.selection.list !== "walls") {
            return false;
        }
        var floor = this.currentFloor();
        var wall = floor.walls[this.selection.index];
        if (!wall) {
            return false;
        }
        var dx = wall.x2 - wall.x1;
        var dz = wall.z2 - wall.z1;
        var len = Math.sqrt(dx * dx + dz * dz);
        if (len < 1e-6) {
            return false;
        }
        var openingLen = Math.min(len * 0.35, (this.model.sceneSize || 1) / 7);
        if (openingLen < len * 0.08) {
            openingLen = len * 0.08;
        }
        var ux = dx / len;
        var uz = dz / len;
        var midX = (wall.x1 + wall.x2) * 0.5;
        var midZ = (wall.z1 + wall.z2) * 0.5;
        var half = openingLen * 0.5;
        var seg = {
            x1: midX - ux * half,
            z1: midZ - uz * half,
            x2: midX + ux * half,
            z2: midZ + uz * half
        };
        var listName = kind === "door" ? "doors" : "windows";
        floor[listName].push(seg);
        this.selection = { list: listName, index: floor[listName].length - 1 };
        this.notify();
        return true;
    };

    SceneDesigner.prototype.removeSelected = function () {
        if (this.selection) {
            if (this.selection.list === "polygon") {
                var floorPolygon = this.currentFloor();
                if (floorPolygon.polygon && floorPolygon.polygon.length > 3
                    && this.selection.index >= 0
                    && this.selection.index < floorPolygon.polygon.length) {
                    floorPolygon.polygon.splice(this.selection.index, 1);
                    this.selection = null;
                    this.notify();
                    this.draw();
                    return true;
                }
                return false;
            }
            var floor = this.currentFloor();
            var list = floor[this.selection.list];
            if (list && this.selection.index >= 0 && this.selection.index < list.length) {
                list.splice(this.selection.index, 1);
                this.selection = null;
                this.notify();
                this.draw();
                return true;
            }
        }
        if (this.model.floors.length > 1) {
            this.model.floors.splice(this.floorIndex, 1);
            if (this.floorIndex >= this.model.floors.length) {
                this.floorIndex = this.model.floors.length - 1;
            }
            this.selection = null;
            this.notify();
            this.draw();
            return true;
        }
        return false;
    };

    SceneDesigner.prototype.currentFloor = function () {
        return this.model.floors[this.floorIndex] || this.model.floors[0];
    };

    SceneDesigner.prototype.worldFromEvent = function (event) {
        return this.worldFromClient(event.clientX, event.clientY);
    };

    SceneDesigner.prototype.worldFromClient = function (clientX, clientY) {
        var rect = this.canvas.getBoundingClientRect();
        if (rect.width <= 0 || rect.height <= 0) {
            return { x: 0, z: 0 };
        }
        var px = (clientX - rect.left) / rect.width;
        var py = (clientY - rect.top) / rect.height;
        var size = this.model.sceneSize || 1;
        return {
            x: (px - 0.5) * size,
            z: (py - 0.5) * size
        };
    };

    SceneDesigner.prototype.handleHitSize = function () {
        return (this.model.sceneSize || 1) * 0.028;
    };

    SceneDesigner.prototype.findHandleAt = function (point) {
        var floor = this.currentFloor();
        var threshold = this.handleHitSize();
        var best = null;
        var bestDist = threshold;
        // Prefer wall/window/door endpoints over floor corners so they stay independent.
        ["walls", "windows", "doors"].forEach(function (listName) {
            var list = floor[listName] || [];
            list.forEach(function (seg, index) {
                [
                    { end: 1, x: seg.x1, z: seg.z1 },
                    { end: 2, x: seg.x2, z: seg.z2 }
                ].forEach(function (handle) {
                    var dx = handle.x - point.x;
                    var dz = handle.z - point.z;
                    var dist = Math.sqrt(dx * dx + dz * dz);
                    if (dist < bestDist) {
                        bestDist = dist;
                        best = { list: listName, index: index, end: handle.end };
                    }
                });
            });
        });
        if (best) {
            return best;
        }
        (floor.polygon || []).forEach(function (corner, index) {
            var dx = corner.x - point.x;
            var dz = corner.z - point.z;
            var dist = Math.sqrt(dx * dx + dz * dz);
            if (dist < bestDist) {
                bestDist = dist;
                best = { list: "polygon", index: index, end: 0 };
            }
        });
        return best;
    };

    SceneDesigner.prototype.snapJoin = function (point, ignoreHandle) {
        var floor = this.currentFloor();
        var threshold = (this.model.sceneSize || 1) * 0.03;
        var best = null;
        var bestDist = threshold;
        ["walls", "windows", "doors"].forEach(function (listName) {
            var list = floor[listName] || [];
            list.forEach(function (seg, index) {
                [
                    { end: 1, x: seg.x1, z: seg.z1 },
                    { end: 2, x: seg.x2, z: seg.z2 }
                ].forEach(function (endpoint) {
                    if (ignoreHandle
                        && ignoreHandle.list === listName
                        && ignoreHandle.index === index
                        && ignoreHandle.end === endpoint.end) {
                        return;
                    }
                    var dx = endpoint.x - point.x;
                    var dz = endpoint.z - point.z;
                    var dist = Math.sqrt(dx * dx + dz * dz);
                    if (dist < bestDist) {
                        bestDist = dist;
                        best = { x: endpoint.x, z: endpoint.z };
                    }
                });
            });
        });
        return best || point;
    };

    SceneDesigner.prototype.selectNear = function (point) {
        var floor = this.currentFloor();
        var threshold = (this.model.sceneSize || 1) * 0.025;
        var best = null;
        var bestDist = threshold;
        ["walls", "windows", "doors"].forEach(function (listName) {
            var list = floor[listName] || [];
            list.forEach(function (seg, index) {
                var dist = distPointToSeg(point.x, point.z, seg);
                if (dist < bestDist) {
                    bestDist = dist;
                    best = { list: listName, index: index };
                }
            });
        });
        if (!best) {
            var polygon = floor.polygon || [];
            var index;
            for (index = 0; index < polygon.length; index++) {
                var a = polygon[index];
                var b = polygon[(index + 1) % polygon.length];
                var dist = distPointToSeg(point.x, point.z, {
                    x1: a.x, z1: a.z, x2: b.x, z2: b.z
                });
                if (dist < bestDist) {
                    bestDist = dist;
                    best = { list: "polygon", index: index };
                }
            }
        }
        this.selection = best;
        this.draw();
        return best;
    };

    SceneDesigner.prototype.insertVertexOnWall = function (point) {
        var floor = this.currentFloor();
        var threshold = (this.model.sceneSize || 1) * 0.03;
        var bestIndex = -1;
        var bestDist = threshold;
        var bestProj = null;
        (floor.walls || []).forEach(function (seg, index) {
            var dist = distPointToSeg(point.x, point.z, seg);
            if (dist < bestDist) {
                bestDist = dist;
                bestIndex = index;
                bestProj = projectOnSeg(point.x, point.z, seg);
            }
        });
        if (bestIndex < 0 || !bestProj || bestProj.t <= 0.05 || bestProj.t >= 0.95) {
            return false;
        }
        var wall = floor.walls[bestIndex];
        var second = { x1: bestProj.x, z1: bestProj.z, x2: wall.x2, z2: wall.z2 };
        wall.x2 = bestProj.x;
        wall.z2 = bestProj.z;
        floor.walls.splice(bestIndex + 1, 0, second);
        this.selection = { list: "walls", index: bestIndex + 1 };
        this.notify();
        this.draw();
        return true;
    };

    SceneDesigner.prototype.insertVertexOnFloorEdge = function (point) {
        var floor = this.currentFloor();
        var polygon = floor.polygon || [];
        if (polygon.length < 3) {
            return false;
        }
        var threshold = (this.model.sceneSize || 1) * 0.03;
        var bestIndex = -1;
        var bestDist = threshold;
        var bestProj = null;
        var index;
        for (index = 0; index < polygon.length; index++) {
            var a = polygon[index];
            var b = polygon[(index + 1) % polygon.length];
            var seg = { x1: a.x, z1: a.z, x2: b.x, z2: b.z };
            var dist = distPointToSeg(point.x, point.z, seg);
            if (dist < bestDist) {
                bestDist = dist;
                bestIndex = index;
                bestProj = projectOnSeg(point.x, point.z, seg);
            }
        }
        if (bestIndex < 0 || !bestProj || bestProj.t <= 0.05 || bestProj.t >= 0.95) {
            return false;
        }
        polygon.splice(bestIndex + 1, 0, { x: bestProj.x, z: bestProj.z });
        this.selection = { list: "polygon", index: bestIndex + 1 };
        this.notify();
        this.draw();
        return true;
    };

    SceneDesigner.prototype.onDoubleClick = function (event) {
        if (this.viewMode) {
            return;
        }
        event.preventDefault();
        var point = this.worldFromEvent(event);
        if (this.insertVertexOnWall(point)) {
            return;
        }
        this.insertVertexOnFloorEdge(point);
    };

    SceneDesigner.prototype.onPointerDown = function (event) {
        if (this.viewMode) {
            this.dragOrbit = true;
            this.lastX = event.clientX;
            this.lastY = event.clientY;
            return;
        }
        var point = this.worldFromEvent(event);
        var handle = this.findHandleAt(point);
        if (handle) {
            this.dragHandle = handle;
            this.selection = { list: handle.list, index: handle.index };
            var corner;
            if (handle.list === "polygon") {
                corner = this.currentFloor().polygon[handle.index];
            } else {
                var handleSeg = this.currentFloor()[handle.list][handle.index];
                corner = handle.end === 1
                    ? { x: handleSeg.x1, z: handleSeg.z1 }
                    : { x: handleSeg.x2, z: handleSeg.z2 };
            }
            this.dragCornerFrom = { x: corner.x, z: corner.z };
            this.dragCornerOrigin = { x: corner.x, z: corner.z };
            this.dragShiftHeld = !!event.shiftKey;
            this.lastDragClientX = event.clientX;
            this.lastDragClientY = event.clientY;
            this.draw();
            return;
        }
        // Empty canvas never creates geometry — only toolbar tools add objects.
        this.selectNear(point);
    };

    SceneDesigner.prototype.applyHandleDragAtClient = function (clientX, clientY, shiftHeld) {
        if (!this.dragHandle || this.viewMode) {
            return;
        }
        var floor = this.currentFloor();
        if (!this.dragCornerFrom) {
            return;
        }
        var raw = this.worldFromClient(clientX, clientY);
        var locked = axisLockPoint(this.dragCornerOrigin, raw, shiftHeld);
        var point = smoothToward(this.dragCornerFrom, locked, 0.65);
        if (this.dragHandle.list === "polygon") {
            var corner = floor.polygon && floor.polygon[this.dragHandle.index];
            if (!corner) {
                return;
            }
            corner.x = point.x;
            corner.z = point.z;
            this.dragCornerFrom = { x: point.x, z: point.z };
            this.draw();
            return;
        }
        var list = floor[this.dragHandle.list];
        if (!list || !list[this.dragHandle.index]) {
            return;
        }
        if (this.dragHandle.list === "walls") {
            // Move only this wall's shared wall-corner group — never the floor polygon.
            moveSharedWallCorner(
                floor,
                this.dragCornerFrom.x,
                this.dragCornerFrom.z,
                point.x,
                point.z
            );
            this.dragCornerFrom = { x: point.x, z: point.z };
            this.draw();
            return;
        }
        var seg = list[this.dragHandle.index];
        if (this.dragHandle.end === 1) {
            seg.x1 = point.x;
            seg.z1 = point.z;
        } else {
            seg.x2 = point.x;
            seg.z2 = point.z;
        }
        this.dragCornerFrom = { x: point.x, z: point.z };
        this.draw();
    };

    SceneDesigner.prototype.finishHandleDrag = function (event) {
        if (!this.dragHandle) {
            return;
        }
        var floor = this.currentFloor();
        if (this.dragCornerFrom) {
            var shiftHeld = event ? !!event.shiftKey : this.dragShiftHeld;
            var raw = event
                ? this.worldFromClient(event.clientX, event.clientY)
                : { x: this.dragCornerFrom.x, z: this.dragCornerFrom.z };
            var locked = axisLockPoint(this.dragCornerOrigin, raw, shiftHeld);
            if (this.dragHandle.list === "polygon") {
                var corner = floor.polygon && floor.polygon[this.dragHandle.index];
                if (corner) {
                    corner.x = locked.x;
                    corner.z = locked.z;
                }
            } else {
                var snapped = this.snapJoin(locked, this.dragHandle);
                if (this.dragHandle.list === "walls") {
                    moveSharedWallCorner(
                        floor,
                        this.dragCornerFrom.x,
                        this.dragCornerFrom.z,
                        snapped.x,
                        snapped.z
                    );
                } else {
                    var list = floor[this.dragHandle.list];
                    var seg = list && list[this.dragHandle.index];
                    if (seg) {
                        if (this.dragHandle.end === 1) {
                            seg.x1 = snapped.x;
                            seg.z1 = snapped.z;
                        } else {
                            seg.x2 = snapped.x;
                            seg.z2 = snapped.z;
                        }
                    }
                }
            }
        }
        this.dragHandle = null;
        this.dragCornerFrom = null;
        this.dragCornerOrigin = null;
        this.dragShiftHeld = false;
        this.notify();
        this.draw();
    };

    SceneDesigner.prototype.onPointerMove = function (event) {
        if (this.viewMode && this.dragOrbit) {
            var dx = event.clientX - this.lastX;
            var dy = event.clientY - this.lastY;
            this.lastX = event.clientX;
            this.lastY = event.clientY;
            this.yaw += dx * 0.01;
            this.pitch = Math.max(0.28, Math.min(1.25, this.pitch + dy * 0.01));
            this.draw();
            return;
        }
        if (!this.dragHandle || this.viewMode) {
            return;
        }
        this.lastDragClientX = event.clientX;
        this.lastDragClientY = event.clientY;
        this.dragShiftHeld = !!event.shiftKey;
        this.applyHandleDragAtClient(event.clientX, event.clientY, this.dragShiftHeld);
    };

    SceneDesigner.prototype.notify = function () {
        if (typeof this.onChange === "function") {
            this.onChange();
        }
    };

    SceneDesigner.prototype.drawTop = function () {
        var ctx = this.ctx;
        var width = this.canvas.width;
        var height = this.canvas.height;
        var size = this.model.sceneSize || 1;
        var thicknessPx = Math.max(4, (this.getWallThickness() / size) * Math.min(width, height));
        ctx.fillStyle = "#1b1f2a";
        ctx.fillRect(0, 0, width, height);
        ctx.strokeStyle = "#2e3648";
        ctx.lineWidth = 1;
        for (var g = 0; g <= 10; g++) {
            var gx = (g / 10) * width;
            var gy = (g / 10) * height;
            ctx.beginPath();
            ctx.moveTo(gx, 0);
            ctx.lineTo(gx, height);
            ctx.stroke();
            ctx.beginPath();
            ctx.moveTo(0, gy);
            ctx.lineTo(width, gy);
            ctx.stroke();
        }
        var self = this;
        function toPx(point) {
            return {
                x: ((point.x / size) + 0.5) * width,
                y: ((point.z / size) + 0.5) * height
            };
        }
        function drawFloorPlate(floor, active) {
            var polygon = floor.polygon || [];
            if (polygon.length < 3) {
                return;
            }
            ctx.beginPath();
            polygon.forEach(function (corner, index) {
                var p = toPx(corner);
                if (index === 0) {
                    ctx.moveTo(p.x, p.y);
                } else {
                    ctx.lineTo(p.x, p.y);
                }
            });
            ctx.closePath();
            ctx.fillStyle = hexToRgba(floor.color || DEFAULT_FLOOR_COLOR, active ? 0.38 : 0.2);
            ctx.fill();
            ctx.strokeStyle = active ? "#9fb6c9" : "#5a6a7c";
            ctx.lineWidth = 1.5;
            ctx.stroke();
        }
        this.model.floors.forEach(function (floor, index) {
            drawFloorPlate(floor, index === self.floorIndex);
        });
        function drawSegs(list, listName, color, widthPx) {
            (list || []).forEach(function (seg, index) {
                var selected = self.selection
                    && self.selection.list === listName
                    && self.selection.index === index;
                var a = toPx({ x: seg.x1, z: seg.z1 });
                var b = toPx({ x: seg.x2, z: seg.z2 });
                ctx.strokeStyle = selected ? "#ce93d8" : color;
                ctx.lineWidth = selected ? widthPx + 2 : widthPx;
                ctx.lineCap = "butt";
                ctx.beginPath();
                ctx.moveTo(a.x, a.y);
                ctx.lineTo(b.x, b.y);
                ctx.stroke();
            });
        }
        function drawHandles(list, listName, color) {
            (list || []).forEach(function (seg, index) {
                var selected = self.selection
                    && self.selection.list === listName
                    && self.selection.index === index;
                var show = listName === "walls" || selected;
                if (!show) {
                    return;
                }
                [
                    { x: seg.x1, z: seg.z1 },
                    { x: seg.x2, z: seg.z2 }
                ].forEach(function (endpoint) {
                    var p = toPx(endpoint);
                    var half = 5;
                    ctx.fillStyle = selected ? "#ce93d8" : color;
                    ctx.strokeStyle = "#111";
                    ctx.lineWidth = 1;
                    ctx.fillRect(p.x - half, p.y - half, half * 2, half * 2);
                    ctx.strokeRect(p.x - half, p.y - half, half * 2, half * 2);
                });
            });
        }
        function drawPolygonHandles(polygon) {
            (polygon || []).forEach(function (corner, index) {
                var selected = self.selection
                    && self.selection.list === "polygon"
                    && self.selection.index === index;
                var p = toPx(corner);
                ctx.beginPath();
                ctx.arc(p.x, p.y, selected ? 6 : 5, 0, Math.PI * 2);
                ctx.fillStyle = selected ? "#ce93d8" : "#81c784";
                ctx.fill();
                ctx.strokeStyle = "#111";
                ctx.lineWidth = 1;
                ctx.stroke();
            });
        }
        var floor = this.currentFloor();
        drawSegs(floor.walls, "walls", "#d7dee8", thicknessPx);
        drawSegs(floor.windows, "windows", "#4fc3f7", Math.max(2, thicknessPx * 0.55));
        drawSegs(floor.doors, "doors", "#ffb74d", Math.max(2, thicknessPx * 0.55));
        drawPolygonHandles(floor.polygon);
        drawHandles(floor.walls, "walls", "#eceff1");
        drawHandles(floor.windows, "windows", "#4fc3f7");
        drawHandles(floor.doors, "doors", "#ffb74d");
        ctx.fillStyle = "#9aa4b5";
        ctx.font = "12px sans-serif";
        ctx.fillText(
            "Floor " + (this.floorIndex + 1) + "/" + this.model.floors.length
                + " · tool: " + this.tool
                + " · thickness: " + this.getWallThickness().toFixed(2),
            10,
            18
        );
    };

    SceneDesigner.prototype.drawView = function () {
        var ctx = this.ctx;
        var width = this.canvas.width;
        var height = this.canvas.height;
        ctx.fillStyle = "#10131a";
        ctx.fillRect(0, 0, width, height);
        var gradient = ctx.createLinearGradient(0, height * 0.45, 0, height);
        gradient.addColorStop(0, "rgba(30,36,48,0)");
        gradient.addColorStop(1, "rgba(20,24,34,0.9)");
        ctx.fillStyle = gradient;
        ctx.fillRect(0, 0, width, height);
        var camera = viewProjectionParams(this.model, width, height, this.yaw, this.pitch);
        paintFaces(
            ctx,
            buildSceneFaces(this.model),
            this.yaw,
            this.pitch,
            camera.distance,
            width,
            height,
            camera.pixelsPerUnit,
            { x: camera.centerX, y: camera.centerY, z: camera.centerZ }
        );
        ctx.fillStyle = "#9aa4b5";
        ctx.font = "12px sans-serif";
        ctx.fillText("View scene · drag to orbit", 10, 18);
    };

    SceneDesigner.prototype.draw = function () {
        if (this.viewMode) {
            this.drawView();
        } else {
            this.drawTop();
        }
    };

    SceneDesigner.prototype.bakeJpegBlob = function (quality) {
        var width = 1920;
        var height = 1080;
        var off = document.createElement("canvas");
        off.width = width;
        off.height = height;
        var ctx = off.getContext("2d");
        ctx.fillStyle = "#10131a";
        ctx.fillRect(0, 0, width, height);
        var gradient = ctx.createLinearGradient(0, height * 0.4, 0, height);
        gradient.addColorStop(0, "rgba(30,36,48,0)");
        gradient.addColorStop(1, "rgba(18,22,30,0.95)");
        ctx.fillStyle = gradient;
        ctx.fillRect(0, 0, width, height);
        var yaw = 0.85;
        var pitch = 0.62;
        var camera = viewProjectionParams(this.model, width, height, yaw, pitch);
        paintFaces(
            ctx,
            buildSceneFaces(this.model),
            yaw,
            pitch,
            camera.distance,
            width,
            height,
            camera.pixelsPerUnit,
            { x: camera.centerX, y: camera.centerY, z: camera.centerZ }
        );
        return new Promise(function (resolve) {
            off.toBlob(function (blob) {
                resolve(blob);
            }, "image/jpeg", quality == null ? 0.82 : quality);
        });
    };

    function mountSceneCell(host, sceneId, options) {
        options = options || {};
        host.classList.add("scene-cell");
        host.innerHTML = "";
        var frame = document.createElement("div");
        frame.className = "scene-cell-frame";
        var img = document.createElement("img");
        img.className = "scene-cell-bake";
        img.alt = "Scene";
        var overlays = document.createElement("div");
        overlays.className = "scene-cell-overlays";
        frame.appendChild(img);
        frame.appendChild(overlays);
        host.appendChild(frame);
        if (!sceneId) {
            host.classList.add("scene-cell--empty");
            host.textContent = "No scene";
            return;
        }
        fetch("/api/scenes/view?id=" + encodeURIComponent(sceneId), { cache: "no-store" })
            .then(function (response) {
                if (!response.ok) {
                    throw new Error("scene view failed");
                }
                return response.json();
            })
            .then(function (view) {
                img.src = "/api/scenes/bake?id=" + encodeURIComponent(sceneId) + "&t=" + Date.now();
                overlays.innerHTML = "";
                (view.pins || []).forEach(function (pin) {
                    var pinEl = document.createElement("div");
                    pinEl.className = "scene-cell-pin scene-cell-pin--" + (pin.viewStyle === "gauge" ? "gauge" : "icon");
                    pinEl.style.left = (clamp01(pin.x) * 100) + "%";
                    pinEl.style.top = (clamp01(pin.y) * 100) + "%";
                    if (typeof options.renderPin === "function") {
                        options.renderPin(pinEl, pin);
                    } else {
                        pinEl.title = pin.kind || "pin";
                    }
                    overlays.appendChild(pinEl);
                });
            })
            .catch(function () {
                host.classList.add("scene-cell--empty");
                host.textContent = "Scene unavailable";
            });
    }

    global.SceneKit = {
        defaultModel: defaultModel,
        SceneDesigner: SceneDesigner,
        mountSceneCell: mountSceneCell,
        clamp01: clamp01
    };
})(window);
