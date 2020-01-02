# -*- mode: ruby -*-
# vi: set ft=ruby :

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  # default build target is currently Ubuntu 18.04
  config.vm.define "bionic", :primary => true do |bionic|
    bionic.vm.box = 'ubuntu/bionic64'
    bionic.vm.provision 'shell', path: 'scripts/provision.sh'
  end

  config.vm.synced_folder ".", "/vagrant"

  config.vm.provider 'virtualbox' do |vb|
    vb.customize ['modifyvm', :id, '--memory', '2048']
  end
end
