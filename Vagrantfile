# -*- mode: ruby -*-
# vi: set ft=ruby :

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  # leave the below as-is to use Ubuntu 14.04
  config.vm.box = 'ubuntu/trusty64'
  config.vm.provision 'shell', path: 'scripts/provision.sh'

  # comment out the two lines above and uncomment the two
  # below to get an F21 box instead.
  #config.vm.box = 'hansode/fedora-21-server-x86_64'
  #config.vm.provision 'shell', path: 'scripts/provision_f21.sh'

  config.vm.provider 'virtualbox' do |vb|
    vb.customize ['modifyvm', :id, '--memory', '2048']
  end
end
