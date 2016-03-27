# -*- mode: ruby -*-
# vi: set ft=ruby :

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  # default build target is currently Ubuntu 14.04
  config.vm.define "trusty", :primary => true do |trusty|
    trusty.vm.box = 'ubuntu/trusty64'
    trusty.vm.provision 'shell', path: 'scripts/provision.sh'
  end

  # but also try to handle F21
  config.vm.define "f21", :autostart => false do |f21|
    f21.vm.box = 'hansode/fedora-21-server-x86_64'
    f21.vm.provision 'shell', path: 'scripts/provision_f21.sh'
  end

  # and Ubuntu 16.04
  config.vm.define "xenial", :autostart => false do |xenial|
    xenial.vm.box = 'ubuntu/xenial64'
    xenial.vm.provision 'shell', path: 'scripts/provision.sh'
  end

  config.vm.synced_folder ".", "/vagrant"

  config.vm.provider 'virtualbox' do |vb|
    vb.customize ['modifyvm', :id, '--memory', '2048']
  end
end
