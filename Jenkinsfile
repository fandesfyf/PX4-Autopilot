pipeline {
  agent none
  stages {
    stage('Quality Checks') {
      agent {
        docker {
          image 'px4io/px4-dev-base:2017-10-23'
          args '--env CI=true'
        }
      }
      steps {
        sh 'make check_format'
      }
    }

    stage('Build') {
      steps {
        script {
          def builds = [:]


          // nuttx default targets that are archived and uploaded to s3
          for (def node_name in ["px4fmu-v4", "px4fmu-v4pro", "px4fmu-v5", "aerofc-v1"]) {
            builds["${node_name}"] = {
              node {
                stage("Build Test ${node_name}") {
                  docker.image('px4io/px4-dev-nuttx:2017-10-23').inside('-e CCACHE_BASEDIR=$WORKSPACE -e CCACHE_DIR=/tmp/ccache -v /tmp/ccache:/tmp/ccache:rw') {
                    stage("${node_name}") {
                      checkout scm
                      sh "make clean"
                      sh "ccache -z"
                      sh "git fetch --tags"
                      sh "make nuttx_${node_name}_default"
                      sh "make nuttx_${node_name}_rtps"
                      sh "ccache -s"
                      archiveArtifacts(artifacts: 'build/*/*.px4', fingerprint: true)
                    }
                  }
                }
              }
            }
          }


          // special case for fmu-v2/fmu-v3
          builds["px4fmu-v2"] = {
            node {
              stage("Build Test ${node_name}") {
                docker.image('px4io/px4-dev-nuttx:2017-10-23').inside('-e CCACHE_BASEDIR=$WORKSPACE -e CCACHE_DIR=/tmp/ccache -v /tmp/ccache:/tmp/ccache:rw') {
                  stage("${node_name}") {
                    checkout scm
                    sh "make clean"
                    sh "ccache -z"
                    sh "git fetch --tags"
                    sh "make nuttx_px4io-v2_default"
                    sh "make nuttx_px4fmu-v2_default"
                    sh "make nuttx_px4fmu-v2_lpe"
                    sh "make nuttx_px4fmu-v3_default"
                    sh "make nuttx_px4fmu-v3_rtps"
                    sh "ccache -s"
                    archiveArtifacts(artifacts: 'build/*/*.px4', fingerprint: true)
                  }
                }
              }
            }
          }


          // nuttx default targets that are archived and uploaded to s3
          for (def node_name in ["aerocore2", "auav-x21", "crazyflie", "mindpx-v2", "nxphlite-v3", "tap-v1"]) {
            builds["${node_name}"] = {
              node {
                stage("Build Test ${node_name}") {
                  docker.image('px4io/px4-dev-nuttx:2017-10-23').inside('-e CCACHE_BASEDIR=$WORKSPACE -e CCACHE_DIR=/tmp/ccache -v /tmp/ccache:/tmp/ccache:rw') {
                    stage("${node_name}") {
                      checkout scm
                      sh "make clean"
                      sh "ccache -z"
                      sh "git fetch --tags"
                      sh "make nuttx_${node_name}_default"
                      sh "ccache -s"
                      archiveArtifacts(artifacts: 'build/*/*.px4', fingerprint: true)
                    }
                  }
                }
              }
            }
          }


          // other nuttx default targets
          for (def node_name in ["px4-same70xplained-v1", "px4-stm32f4discovery", "px4cannode-v1", "px4esc-v1", "px4nucleoF767ZI-v1", "s2740vc-v1"]) {
            builds["${node_name}"] = {
              node {
                stage("Build Test ${node_name}") {
                  docker.image('px4io/px4-dev-nuttx:2017-10-23').inside('-e CCACHE_BASEDIR=$WORKSPACE -e CCACHE_DIR=/tmp/ccache -v /tmp/ccache:/tmp/ccache:rw') {
                    stage("${node_name}") {
                      checkout scm
                      sh "make clean"
                      sh "ccache -z"
                      sh "make nuttx_${node_name}_default"
                      sh "ccache -s"
                    }
                  }
                }
              }
            }
          }


          // raspberry pi and bebop (armhf)
          for (def node_name in ["rpi_cross", "bebop_default"]) {
            builds["${node_name}"] = {
              node {
                stage("Build Test ${node_name}") {
                  docker.image('px4io/px4-dev-raspi:2017-10-23').inside('-e CCACHE_BASEDIR=$WORKSPACE -e CCACHE_DIR=/tmp/ccache -v /tmp/ccache:/tmp/ccache:rw') {
                    stage("${node_name}") {
                      checkout scm
                      sh "make clean"
                      sh "ccache -z"
                      sh "make posix_${node_name}"
                      sh "ccache -s"
                    }
                  }
                }
              }
            }
          }


          // other armhf (to be merged with raspi and bebop)
          for (def node_name in ["ocpoc_ubuntu"]) {
            builds["${node_name}"] = {
              node {
                stage("Build Test ${node_name}") {
                  docker.image('px4io/px4-dev-armhf:2017-10-23').inside('-e CCACHE_BASEDIR=$WORKSPACE -e CCACHE_DIR=/tmp/ccache -v /tmp/ccache:/tmp/ccache:rw') {
                    stage("${node_name}") {
                      checkout scm
                      sh "make clean"
                      sh "ccache -z"
                      sh "make posix_${node_name}"
                      sh "ccache -s"
                    }
                  }
                }
              }
            }
          }


          // GCC7 tests
          for (def node_name in ["posix_sitl_default", "nuttx_px4fmu-v5_default"]) {
            builds["${node_name} (GCC7)"] = {
              node {
                stage("Build Test ${node_name} (GCC7)") {
                  docker.image('px4io/px4-dev-base-archlinux:2017-12-08').inside('-e CCACHE_BASEDIR=$WORKSPACE -e CCACHE_DIR=/tmp/ccache -v /tmp/ccache:/tmp/ccache:rw') {
                    stage("${node_name}") {
                      checkout scm
                      sh "make clean"
                      sh "ccache -z"
                      sh "make ${node_name}"
                      sh "ccache -s"
                    }
                  }
                }
              }
            }
          }

          parallel builds
        }
      }
    }

    stage('Test') {
      parallel {

        // temporarily disabled until build resources are available
        //stage('clang-tidy') {
        //  agent {
        //    docker {
        //      image 'px4io/px4-dev-clang:2017-10-23'
        //      args '-e CCACHE_BASEDIR=$WORKSPACE -e CCACHE_DIR=/tmp/ccache -v /tmp/ccache:/tmp/ccache:rw'
        //    }
        //  }
        //  steps {
        //    sh 'make clean'
        //    sh 'make clang-tidy-quiet'
        //  }
        //}

        stage('tests') {
          agent {
            docker {
              image 'px4io/px4-dev-base:2017-10-23'
              args '-e CCACHE_BASEDIR=$WORKSPACE -e CCACHE_DIR=/tmp/ccache -v /tmp/ccache:/tmp/ccache:rw'
            }
          }
          steps {
            sh 'make clean'
            sh 'make posix_sitl_default test_results_junit'
            junit 'build/posix_sitl_default/JUnitTestResults.xml'
          }
        }

        // temporarily disabled until stable
        //stage('tests coverage') {
        //  agent {
        //    docker {
        //      image 'px4io/px4-dev-base:2017-10-23'
        //      args '-e CCACHE_BASEDIR=$WORKSPACE -e CCACHE_DIR=/tmp/ccache -v /tmp/ccache:/tmp/ccache:rw'
        //    }
        //  }
        //  steps {
        //    sh 'make clean'
        //    sh 'make tests_coverage'
        //    // publish html
        //    publishHTML target: [
        //      allowMissing: false,
        //      alwaysLinkToLastBuild: false,
        //      keepAll: true,
        //      reportDir: 'build/posix_sitl_default/coverage-html',
        //      reportFiles: '*',
        //      reportName: 'Coverage Report'
        //    ]
        //  }
        //}

      }
    }

    stage('Generate Metadata') {

      parallel {

        stage('airframe') {
          agent {
            docker { image 'px4io/px4-dev-base:2017-10-23' }
          }
          steps {
            sh 'make airframe_metadata'
            archiveArtifacts(artifacts: 'airframes.md, airframes.xml', fingerprint: true)
          }
        }

        stage('parameter') {
          agent {
            docker { image 'px4io/px4-dev-base:2017-10-23' }
          }
          steps {
            sh 'make parameters_metadata'
            archiveArtifacts(artifacts: 'parameters.md, parameters.xml', fingerprint: true)
          }
        }

        stage('module') {
          agent {
            docker { image 'px4io/px4-dev-base:2017-10-23' }
          }
          steps {
            sh 'make module_documentation'
            archiveArtifacts(artifacts: 'modules/*.md', fingerprint: true)
          }
        }
      }
    }

    stage('S3 Upload') {
      agent {
        docker { image 'px4io/px4-dev-base:2017-10-23' }
      }

      when {
        anyOf {
          branch 'master'
          branch 'beta'
          branch 'stable'
        }
      }

      steps {
        sh 'echo "uploading to S3"'
      }
    }
  }

  options {
    buildDiscarder(logRotator(numToKeepStr: '5'))
    timeout(time: 60, unit: 'MINUTES')
  }
}
