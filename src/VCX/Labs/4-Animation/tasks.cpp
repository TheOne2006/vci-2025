#include "Labs/4-Animation/tasks.h"
#include "CustomFunc.inl"
#include "IKSystem.h"
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <spdlog/spdlog.h>

namespace VCX::Labs::Animation {
    void ForwardKinematics(IKSystem & ik, int StartIndex) {
        if (StartIndex == 0) {
            ik.JointGlobalRotation[0] = ik.JointLocalRotation[0];
            ik.JointGlobalPosition[0] = ik.JointLocalOffset[0];
            StartIndex                = 1;
        }

        for (int i = StartIndex; i < ik.JointLocalOffset.size(); i++) {
            // your code here: forward kinematics, update JointGlobalPosition and JointGlobalRotation
            ik.JointGlobalPosition[i] = ik.JointGlobalPosition[i - 1] + ik.JointGlobalRotation[i - 1] * ik.JointLocalOffset[i];
            ik.JointGlobalRotation[i] = ik.JointGlobalRotation[i - 1] * ik.JointLocalRotation[i];
        }
    }

    void InverseKinematicsCCD(IKSystem & ik, const glm::vec3 & EndPosition, int maxCCDIKIteration, float eps) {
        ForwardKinematics(ik, 0);
        // These functions will be useful: glm::normalize, glm::rotation, glm::quat * glm::quat
        for (int CCDIKIteration = 0; CCDIKIteration < maxCCDIKIteration && glm::l2Norm(ik.EndEffectorPosition() - EndPosition) > eps; CCDIKIteration++) {
            // your code here: ccd ik
            for (int joint_id = ik.JointLocalOffset.size() - 1; joint_id >= 0; joint_id--) {
                glm::vec3 end_position   = ik.EndEffectorPosition();
                glm::vec3 joint_position = ik.JointGlobalPosition[joint_id];
                glm::vec3 now_direction  = glm::normalize(end_position - joint_position);
                glm::vec3 direction      = glm::normalize(EndPosition - joint_position);

                auto rot                        = glm::rotation(now_direction, direction);
                ik.JointLocalRotation[joint_id] = rot * ik.JointLocalRotation[joint_id];
                ForwardKinematics(ik, joint_id);
            }
        }
    }

    void InverseKinematicsFABR(IKSystem & ik, const glm::vec3 & EndPosition, int maxFABRIKIteration, float eps) {
        ForwardKinematics(ik, 0);
        int                    nJoints            = ik.NumJoints();
        std::vector<glm::vec3> backward_positions = ik.JointGlobalPosition, forward_positions(nJoints, glm::vec3(0, 0, 0));
        for (int IKIteration = 0; IKIteration < maxFABRIKIteration && glm::l2Norm(ik.EndEffectorPosition() - EndPosition) > eps; IKIteration++) {
            // task: fabr ik
            // backward stage
            backward_positions[nJoints - 1] = EndPosition;
            for (int i = nJoints - 2; i >= 0; i--) {
                glm::vec3 dir         = glm::normalize(ik.JointGlobalPosition[i] - backward_positions[i + 1]);
                backward_positions[i] = backward_positions[i + 1] + dir * ik.JointOffsetLength[i + 1];
            }

            // forward stage
            forward_positions[0] = ik.JointGlobalPosition[0];
            for (int i = 0; i < nJoints - 1; i++) {
                glm::vec3 dir            = glm::normalize(backward_positions[i + 1] - forward_positions[i]);
                forward_positions[i + 1] = forward_positions[i] + dir * ik.JointOffsetLength[i + 1];
            }

            ik.JointGlobalPosition = forward_positions; // copy forward positions to joint_positions
        }

        // Compute joint rotation by position here.
        for (int i = 0; i < nJoints - 1; i++) {
            ik.JointGlobalRotation[i] = glm::rotation(glm::normalize(ik.JointLocalOffset[i + 1]), glm::normalize(ik.JointGlobalPosition[i + 1] - ik.JointGlobalPosition[i]));
        }
        ik.JointLocalRotation[0] = ik.JointGlobalRotation[0];
        for (int i = 1; i < nJoints - 1; i++) {
            ik.JointLocalRotation[i] = glm::inverse(ik.JointGlobalRotation[i - 1]) * ik.JointGlobalRotation[i];
        }
        ForwardKinematics(ik, 0);
    }

    IKSystem::Vec3ArrPtr IKSystem::BuildCustomTargetPosition() {
        bool useEinstein = true;

        int nums      = 5000;
        using Vec3Arr = std::vector<glm::vec3>;
        IKSystem::Vec3ArrPtr custom(new Vec3Arr());
        int                  index = 0;

        if (useEinstein) {
            // Einstein 曲线模式 - 自适应采样
            // get function from https://www.wolframalpha.com/input/?i=Albert+Einstein+curve

            float epsilon          = 0.03f;
            int   max_subdivisions = 4;
            int   m                = 10;

            auto getPoint = [](float t) -> glm::vec3 {
                float x_val = 1.5e-3f * custom_x(t);
                float y_val = 1.5e-3f * custom_y(t);
                return glm::vec3(1.6f - x_val, 0.0f, y_val - 0.2f);
            };

            std::function<void(float, float, int, std::vector<glm::vec3> &)> subdivide;
            subdivide = [&](float t0, float t1, int depth, std::vector<glm::vec3> & points) {
                glm::vec3 p0   = getPoint(t0);
                glm::vec3 p1   = getPoint(t1);
                float     dist = glm::length(p1 - p0);

                if (dist < epsilon || depth >= max_subdivisions) {
                    if (std::abs(p0.x - 1.6f) > 1e-3 && std::abs(p0.z + 0.2f) > 1e-3) 
                        points.push_back(p0);
                    return;
                }

                float step = (t1 - t0) / (m + 1);
                for (int i = 0; i <= m; ++i) {
                    float t_mid  = t0 + i * step;
                    float t_next = t0 + (i + 1) * step;
                    subdivide(t_mid, t_next, depth + 1, points);
                }
            };

            std::vector<glm::vec3> temp_points;
            int                    initial_samples = 1000;
            float                  t_max           = 92 * glm::pi<float>();

            for (int i = 0; i < initial_samples; i++) {
                float t0 = t_max * i / initial_samples;
                float t1 = t_max * (i + 1) / initial_samples;
                subdivide(t0, t1, 0, temp_points);
            }

            glm::vec3 last = getPoint(t_max);
            if (std::abs(last.x - 1.6f) > 1e-3 && std::abs(last.z + 0.2f) > 1e-3) {
                temp_points.push_back(last);
            }

            *custom = temp_points;
            printf("Final Size: %ld", custom->size());
            return custom;
        } else {
            // WTXY 字母模式
            float spacing = 0.5f;
            float scale   = 1.0f;
            auto  lerp    = [](glm::vec3 a, glm::vec3 b, float t) -> glm::vec3 {
                return a + t * (b - a);
            };
            std::vector<std::vector<glm::vec3>> letters;
            // W
            letters.push_back({
                {    0, 0,    0 },
                {    0, 0,    1 },
                { 0.2f, 0, 0.5f },
                { 0.4f, 0,    1 },
                { 0.4f, 0,    0 }
            });
            // T
            letters.push_back({
                {    0, 0, 0 },
                { 0.4f, 0, 0 },
                { 0.2f, 0, 0 },
                { 0.2f, 0, 1 }
            });
            // X
            letters.push_back({
                {    0, 0,    0 },
                { 0.4f, 0,    1 },
                { 0.2f, 0, 0.5f },
                {    0, 0,    1 },
                { 0.4f, 0,    0 }
            });
            // Y
            letters.push_back({
                {    0, 0,    0 },
                { 0.2f, 0, 0.5f },
                { 0.4f, 0,    0 },
                { 0.2f, 0, 0.5f },
                { 0.2f, 0,    1 }
            });
            
            std::vector<glm::vec3> temp_points;
            temp_points.reserve(nums);  // 预留空间
            
            glm::vec3 offset(0, 0, 0);
            for (auto & letter : letters) {
                int seg_points = nums / (letters.size() * letter.size());
                for (size_t j = 0; j < letter.size() - 1; j++) {
                    for (int k = 0; k < seg_points; k++) {
                        float     t     = float(k) / seg_points;
                        glm::vec3 point = lerp(letter[j], letter[j + 1], t) * scale + offset;
                        temp_points.push_back(point);
                    }
                }
                offset.x += spacing;
            }
            
            *custom = temp_points;
            return custom;
        }
    }

    static Eigen::VectorXf glm2eigen(std::vector<glm::vec3> const & glm_v) {
        Eigen::VectorXf v = Eigen::Map<Eigen::VectorXf const, Eigen::Aligned>(reinterpret_cast<float const *>(glm_v.data()), static_cast<int>(glm_v.size() * 3));
        return v;
    }

    static std::vector<glm::vec3> eigen2glm(Eigen::VectorXf const & eigen_v) {
        return std::vector<glm::vec3>(
            reinterpret_cast<glm::vec3 const *>(eigen_v.data()),
            reinterpret_cast<glm::vec3 const *>(eigen_v.data() + eigen_v.size()));
    }

    static Eigen::SparseMatrix<float> CreateEigenSparseMatrix(std::size_t n, std::vector<Eigen::Triplet<float>> const & triplets) {
        Eigen::SparseMatrix<float> matLinearized(n, n);
        matLinearized.setFromTriplets(triplets.begin(), triplets.end());
        return matLinearized;
    }

    // solve Ax = b and return x
    static Eigen::VectorXf ComputeSimplicialLLT(
        Eigen::SparseMatrix<float> const & A,
        Eigen::VectorXf const &            b) {
        auto solver = Eigen::SimplicialLLT<Eigen::SparseMatrix<float>>(A);
        return solver.solve(b);
    }

    void AdvanceMassSpringSystem(MassSpringSystem & system, float const dt) {
        // your code here: rewrite following code
        float const fixed_mass  = 1e8;
        int const   steps       = 2;
        float const h           = dt / steps;
        auto const  k           = system.Stiffness;
        auto const  d           = system.Damping; // 阻尼系数
        auto const  num         = system.Positions.size();
        float       inv_h2      = 1.0f / (h * h);
        glm::vec3   gravity_vec = glm::vec3(0.0f, -system.Gravity, 0.0f);

        for (std::size_t s = 0; s < steps; ++s) {
            // Stage1: y_k
            auto &                 x_k = system.Positions;
            auto &                 v_k = system.Velocities;
            std::vector<glm::vec3> y_k(num);
            for (std::size_t i = 0; i < num; ++i) {
                // M^{-1} f_ext = gravity_vec (if f_ext = m * gravity_vec)
                if (system.Fixed[i]) y_k[i] = x_k[i] + h * v_k[i]; // 固定点不加重力
                else y_k[i] = x_k[i] + h * v_k[i] + h * h * gravity_vec;
            }

            // Stage2: nabla_E (internal forces gradient of E + damping)
            std::vector<glm::vec3> nabla_E(num, glm::vec3(0.0f));
            for (auto & spring : system.Springs) {
                auto [i1, j1]     = spring.AdjIdx;
                glm::vec3 delta_x = x_k[j1] - x_k[i1];
                float     length  = glm::length(delta_x);
                if (length < 1e-6f) continue;
                glm::vec3 dir = delta_x / length;

                // spring force from i->j:
                glm::vec3 f_ij = k * (length - spring.RestLength) * dir;
                nabla_E[i1] += -f_ij;
                nabla_E[j1] += f_ij;

                // damping force along spring direction
                glm::vec3 v_rel  = v_k[j1] - v_k[i1];
                glm::vec3 f_damp = d * glm::dot(v_rel, dir) * dir; // 阻尼只沿弹簧方向
                nabla_E[i1] += -f_damp;
                nabla_E[j1] += f_damp;
            }

            // nabla_g = (1/h^2) * M * (x_k - y_k) + nabla_E
            std::vector<glm::vec3> nabla_g(num);
            for (std::size_t i = 0; i < num; ++i) {
                if (system.Fixed[i]) nabla_g[i] = inv_h2 * fixed_mass * (x_k[i] - y_k[i]) + nabla_E[i];
                else nabla_g[i] = inv_h2 * system.Mass * (x_k[i] - y_k[i]) + nabla_E[i];
            }

            // Stage3: assemble H_E (Hessian of E)
            std::vector<Eigen::Triplet<float>> triplets;
            triplets.reserve(system.Springs.size() * 12); // approx 12 triplets per spring
            for (auto & spring : system.Springs) {
                auto [i1, j1]     = spring.AdjIdx;
                glm::vec3 delta_x = x_k[j1] - x_k[i1];
                float     length  = glm::length(delta_x);
                if (length < 1e-6f) continue;
                Eigen::Vector3f dx(delta_x.x, delta_x.y, delta_x.z);
                Eigen::Matrix3f outer = dx * dx.transpose() / (length * length);
                Eigen::Matrix3f I     = Eigen::Matrix3f::Identity();
                Eigen::Matrix3f H_e   = k * outer
                    + k * (1.0f - spring.RestLength / length) * (I - outer);

                for (int a = 0; a < 3; ++a)
                    for (int b = 0; b < 3; ++b) {
                        float v = H_e(a, b);
                        triplets.emplace_back(3 * i1 + a, 3 * i1 + b, v);
                        triplets.emplace_back(3 * j1 + a, 3 * j1 + b, v);
                        triplets.emplace_back(3 * i1 + a, 3 * j1 + b, -v);
                        triplets.emplace_back(3 * j1 + a, 3 * i1 + b, -v);
                    }
            }

            Eigen::SparseMatrix<float> H_E(3 * num, 3 * num);
            H_E.setFromTriplets(triplets.begin(), triplets.end());

            // Stage4: Build A = (1/h^2) * M + H_E
            // make a sparse diagonal mass matrix M_sparse (3n x 3n)
            std::vector<Eigen::Triplet<float>> massTrip;
            massTrip.reserve(3 * num);
            for (std::size_t i = 0; i < num; ++i) {
                float m;
                if (system.Fixed[i]) m = fixed_mass;
                else m = system.Mass;
                // diagonal on x,y,z
                massTrip.emplace_back(3 * i + 0, 3 * i + 0, m * inv_h2);
                massTrip.emplace_back(3 * i + 1, 3 * i + 1, m * inv_h2);
                massTrip.emplace_back(3 * i + 2, 3 * i + 2, m * inv_h2);
            }
            Eigen::SparseMatrix<float> M_sparse(3 * num, 3 * num);
            M_sparse.setFromTriplets(massTrip.begin(), massTrip.end());

            Eigen::SparseMatrix<float> A = H_E;
            A += M_sparse; // now A = H_E + (1/h^2) M

            // Stage5: RHS and Newton solve: A * delta_x = -nabla_g
            Eigen::VectorXf rhs     = -glm2eigen(nabla_g);
            Eigen::VectorXf delta_x = ComputeSimplicialLLT(A, rhs);

            // Stage6: update x
            Eigen::VectorXf xk_vec     = glm2eigen(x_k);
            Eigen::VectorXf xkp_vec    = xk_vec + delta_x;
            auto            x_k_plus_1 = eigen2glm(xkp_vec);

            // updated positions
            for (size_t i = 0; i < num; ++i) {
                if (system.Fixed[i]) continue;
                system.Positions[i] = x_k_plus_1[i];
            }

            // optionally update velocities: v_{k+1} = (x_{k+1} - x_k) / h
            auto  delta_xp    = eigen2glm(delta_x);
            float total_speed = 0.0f;
            for (size_t i = 0; i < num; ++i) {
                system.Velocities[i] = delta_xp[i] / h;
            }
        }
    }
} // namespace VCX::Labs::Animation
